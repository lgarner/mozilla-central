/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *   Kyle Machulis <kyle@nonpolynomial.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <queue>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Util.h"
#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsXULAppAPI.h"
#include "Nfc.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif

#define NFC_SOCKET_NAME "/dev/socket/nfcd"

using namespace base;
using namespace std;

// Network port to connect to for adb forwarded sockets when doing
// desktop development.
const uint32_t NFCD_TEST_PORT = 6300;

namespace mozilla {
namespace ipc {

struct NfcClient : public RefCounted<NfcClient>,
                   public MessageLoopForIO::Watcher

{
    typedef queue<NfcData*> NfcDataQueue;

    NfcClient() : mSocket(-1)
                , mMutex("NfcClient.mMutex")
                , mBlockedOnWrite(false)
                , mCurrentNfcData(NULL)
                , mIOLoop(MessageLoopForIO::current())
    { }
    virtual ~NfcClient() { }

    bool OpenSocket();

    virtual void OnFileCanReadWithoutBlocking(int fd);
    virtual void OnFileCanWriteWithoutBlocking(int fd);

    ScopedClose mSocket;
    MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
    MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;
    nsAutoPtr<NfcData> mIncoming;
    Mutex mMutex;
    NfcDataQueue mOutgoingQ;
    bool mBlockedOnWrite;
    MessageLoopForIO* mIOLoop;
    nsAutoPtr<NfcData> mCurrentNfcData;
    size_t mCurrentWriteOffset;
};

static RefPtr<NfcClient> sClient;
static RefPtr<NfcConsumer> sConsumer;

//-----------------------------------------------------------------------------
// This code runs on the IO thread.
//

class NfcReconnectTask : public CancelableTask {
    NfcReconnectTask() : mCanceled(false) { }

    virtual void Run();
    virtual void Cancel() { mCanceled = true; }

    bool mCanceled;

public:
    static void Enqueue(int aDelayMs = 0) {
        MessageLoopForIO* ioLoop = MessageLoopForIO::current();
        MOZ_ASSERT(ioLoop && sClient->mIOLoop == ioLoop);
        if (sTask) {
            return;
        }
        sTask = new NfcReconnectTask();
        if (aDelayMs) {
            ioLoop->PostDelayedTask(FROM_HERE, sTask, aDelayMs);
        } else {
            ioLoop->PostTask(FROM_HERE, sTask);
        }
    }

    static void CancelIt() {
        if (!sTask) {
            return;
        }
        sTask->Cancel();
        sTask = nsnull;
    }

private:
    // Can *ONLY* be touched by the IO thread.  The event queue owns
    // this memory when pointer is nonnull; do *NOT* free it manually.
    static CancelableTask* sTask;
};
CancelableTask* NfcReconnectTask::sTask;

void NfcReconnectTask::Run() {
    // NB: the order of these two statements is important!  sTask must
    // always run, whether we've been canceled or not, to avoid
    // leading a dangling pointer in sTask.
    sTask = nsnull;
    if (mCanceled) {
        return;
    }

    if (sClient->OpenSocket()) {
        return;
    }
    Enqueue(1000);
}

class NfcWriteTask : public Task {
    virtual void Run();
};

void NfcWriteTask::Run() {
    sClient->OnFileCanWriteWithoutBlocking(sClient->mSocket.rwget());
}

static void
ConnectToNfc(Monitor* aMonitor, bool* aSuccess)
{
    MOZ_ASSERT(!sClient);

    sClient = new NfcClient();
    NfcReconnectTask::Enqueue();
    *aSuccess = true;
    {
        MonitorAutoLock lock(*aMonitor);
        lock.Notify();
    }
    // aMonitor may have gone out of scope by now, don't touch it
}

bool
NfcClient::OpenSocket()
{
#if defined(MOZ_WIDGET_GONK)
    // Using a network socket to test basic functionality
    // before we see how this works on the phone.
    struct sockaddr_un addr;
    socklen_t alen;
    size_t namelen;
    int err;
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, NFC_SOCKET_NAME);
    addr.sun_family = AF_LOCAL;
    mSocket.reset(socket(AF_LOCAL, SOCK_STREAM, 0));
    alen = strlen(NFC_SOCKET_NAME) + offsetof(struct sockaddr_un, sun_path) + 1;
#else
    struct hostent *hp;
    struct sockaddr_in addr;
    socklen_t alen;

    hp = gethostbyname("localhost");
    if (hp == 0) return false;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons(NFCD_TEST_PORT);
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
    mSocket.mFd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    alen = sizeof(addr);
#endif

    if (mSocket.get() < 0) {
        LOG("Cannot create socket for NFC!\n");
        return false;
    }

    if (connect(mSocket.get(), (struct sockaddr *) &addr, alen) < 0) {
#if defined(MOZ_WIDGET_GONK)
        LOG("Cannot open socket for NFC!\n");
#endif
        mSocket.dispose();
        return false;
    }

    // Set close-on-exec bit.
    int flags = fcntl(mSocket.get(), F_GETFD);
    if (-1 == flags) {
        return false;
    }

    flags |= FD_CLOEXEC;
    if (-1 == fcntl(mSocket.get(), F_SETFD, flags)) {
        return false;
    }

    // Select non-blocking IO.
    if (-1 == fcntl(mSocket.get(), F_SETFL, O_NONBLOCK)) {
        return false;
    }
    if (!mIOLoop->WatchFileDescriptor(mSocket.get(),
                                      true,
                                      MessageLoopForIO::WATCH_READ,
                                      &mReadWatcher,
                                      this)) {
        return false;
    }
    LOG("Socket open for NFC\n");
    return true;
}

void
NfcClient::OnFileCanReadWithoutBlocking(int fd)
{
    // Keep reading data until either
    //
    //   - mIncoming is completely read
    //     If so, sConsumer->MessageReceived(mIncoming.forget())
    //
    //   - mIncoming isn't completely read, but there's no more
    //     data available on the socket
    //     If so, break;

    MOZ_ASSERT(fd == mSocket.get());

    size_t byteCount, read_offset;
    while (true) {
        if (!mIncoming) {
            mIncoming = new NfcData();
            mIncoming->json = NULL;

            ssize_t ret = read(fd, &byteCount, sizeof(size_t));
            if (ret <= 0) {
                LOG("Cannot read from network, error %ld\n", ret);
                goto clean_and_return;
            }
            LOG("Reading %d bytes long message", byteCount);

            mIncoming->json = (char *) malloc(byteCount);

            // Read until we reach the byte count
            read_offset = 0;
            while(read_offset < byteCount) {
              int ret = read(fd, mIncoming->json + read_offset, 
                                 byteCount - read_offset);
              if (ret < 0) {
                LOG("Cannot read from network, error %d (errno: %d)\n", ret, errno);
                goto clean_and_return;
              } else {
                read_offset += ret;
              }
            }

            sConsumer->MessageReceived(mIncoming.forget());	
            return;
        }
    }

clean_and_return:
    // At this point, assume that we can't actually access
    // the socket anymore, and start a reconnect loop.
    mIncoming.forget();
    mReadWatcher.StopWatchingFileDescriptor();
    mWriteWatcher.StopWatchingFileDescriptor();
    close(mSocket.get());
    NfcReconnectTask::Enqueue();
    return;
}

void
NfcClient::OnFileCanWriteWithoutBlocking(int fd)
{
    // Try to write the bytes of mCurrentNfcData.  If all were written, continue.
    //
    // Otherwise, save the byte position of the next byte to write
    // within mCurrentNfcData, and request another write when the
    // system won't block.
    //

    MOZ_ASSERT(fd == mSocket.get());

    while (!mOutgoingQ.empty() || mCurrentNfcData != NULL) {
        if(!mCurrentNfcData) {
            mCurrentNfcData = mOutgoingQ.front();
            mOutgoingQ.pop();
            mCurrentWriteOffset = 0;
        }
        char *toWrite = mCurrentNfcData->json;
        size_t write_amount = strlen(toWrite) + 1;

        LOG("Writing %d bytes to nfcd (%s)\n", write_amount, toWrite);
        ssize_t written = write (fd, &write_amount, sizeof(size_t));
        if(written < 0) {
              LOG("Cannot write to network, error %ld\n", written);
              goto clean_and_return;
        }
 
        while (mCurrentWriteOffset < write_amount) {
              written = write (fd, toWrite + mCurrentWriteOffset, 
                                         write_amount - mCurrentWriteOffset);
            if(written < 0) {
              LOG("Cannot write to network, error %ld\n", written);
              goto clean_and_return;
            } else {
              mCurrentWriteOffset += written;
            }
//            LOG("XXXXXXXXXXXXXXXXXXXXXXX written:%ld offset:%ld\n", written, mCurrentWriteOffset);
        }

        if(mCurrentWriteOffset != write_amount) {
            MessageLoopForIO::current()->WatchFileDescriptor(
                fd,
                false,
                MessageLoopForIO::WATCH_WRITE,
                &mWriteWatcher,
                this);
            goto clean_and_return;
        }
        free(mCurrentNfcData->json);
        mCurrentNfcData = NULL;
    }
    return;

clean_and_return:
    free(mCurrentNfcData->json);
    mCurrentNfcData = NULL;
    return;
}


static void
DisconnectFromNfc(Monitor* aMonitor)
{
    // Prevent stale reconnect tasks from being run after we've shut
    // down.
    NfcReconnectTask::CancelIt();
    // XXX This might "strand" messages in the outgoing queue.  We'll
    // assume that's OK for now.
    sClient = nsnull;
    {
        MonitorAutoLock lock(*aMonitor);
        lock.Notify();
    }
}

//-----------------------------------------------------------------------------
// This code runs on any thread.
//

bool
StartNfc(NfcConsumer* aConsumer)
{
    MOZ_ASSERT(aConsumer);
    sConsumer = aConsumer;

    Monitor monitor("StartNfc.monitor");
    bool success;
    {
        MonitorAutoLock lock(monitor);

        XRE_GetIOMessageLoop()->PostTask(
            FROM_HERE,
            NewRunnableFunction(ConnectToNfc, &monitor, &success));

        lock.Wait();
    }

    return success;
}

bool
SendNfcData(NfcData** aMessage)
{
    if (!sClient) {
        return false;
    }

    NfcData *msg = *aMessage;
    *aMessage = nsnull;

    {
        MutexAutoLock lock(sClient->mMutex);
        sClient->mOutgoingQ.push(msg);
    }
    sClient->mIOLoop->PostTask(FROM_HERE, new NfcWriteTask());

    return true;
}

void
StopNfc()
{
    Monitor monitor("StopNfc.monitor");
    {
        MonitorAutoLock lock(monitor);

        XRE_GetIOMessageLoop()->PostTask(
            FROM_HERE,
            NewRunnableFunction(DisconnectFromNfc, &monitor));

        lock.Wait();
    }

    sConsumer = nsnull;
}


} // namespace ipc
} // namespace mozilla
