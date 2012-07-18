/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#include "Nfc.h"
#include "NdefRecord.h"

#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsPIDOMWindow.h"
#include "nsJSON.h"
#include "nsStringBuffer.h"

#include "jsapi.h"
#include "json.h"
#include "mozilla/Preferences.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "SystemWorkerManager.h"
#include "mozilla/LazyIdleThread.h"

#include "NfcNdefEvent.h"

USING_NFC_NAMESPACE
using mozilla::Preferences;

#define DOM_TELEPHONY_APP_PHONE_URL_PREF "dom.telephony.app.phone.url"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...) 
#endif

Nfc::Nfc()
{
}

Nfc::~Nfc()
{
  if (mNfc && mNFCCallback) {
    mNfc->UnregisterCallback(mNFCCallback);
  }
}

// static
already_AddRefed<Nfc>
Nfc::Create(nsPIDOMWindow* aOwner, nsINfc* aNfc)
{
  NS_ASSERTION(aOwner, "Null owner!");
  NS_ASSERTION(aNfc, "Null NFC!");

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aOwner);
  NS_ENSURE_TRUE(sgo, nsnull);

  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  NS_ENSURE_TRUE(scriptContext, nsnull);

  nsRefPtr<Nfc> nfc = new Nfc();

  nfc->BindToOwner(aOwner);

  nfc->mNfc = aNfc;
  nfc->mNFCCallback = new NFCCallback(nfc);

  nsresult rv = aNfc->RegisterCallback(nfc->mNFCCallback);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return nfc.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Nfc)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Nfc,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(ndefdiscovered)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(taglost)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Nfc,
                                               nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Nfc,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(ndefdiscovered)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(taglost)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Nfc)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNfc)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Nfc)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Nfc, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Nfc, nsDOMEventTargetHelper)

DOMCI_DATA(Nfc, Nfc)

NS_IMPL_ISUPPORTS1(Nfc::NFCCallback, nsINFCCallback)

NS_IMPL_EVENT_HANDLER(Nfc, ndefdiscovered)
NS_IMPL_EVENT_HANDLER(Nfc, taglost)

NS_IMETHODIMP
Nfc::NdefDiscovered(const nsAString &aNdefRecords)
{
  //NS_ASSERTION(aNdefRecords, "Null records!");
  LOG("DOM: Received NdefDiscovered callback => create event");

  // Parse JSON
  jsval result;
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  int length = aNdefRecords.Length();
  if ((length <= 0) || !JS_ParseJSON(sc->GetNativeContext(), static_cast<const jschar*>(PromiseFlatString(aNdefRecords).get()),
       aNdefRecords.Length(), &result)) {
    LOG("DOM: Couldn't parse JSON");
    return NS_ERROR_UNEXPECTED;
  }

  // Dispatch incoming event.
  nsRefPtr<NfcNdefEvent> event = NfcNdefEvent::Create(result);
  NS_ASSERTION(event, "This should never fail!");

  LOG("DOM: Created event => dispatching");
  rv = event->Dispatch(ToIDOMEventTarget(), NS_LITERAL_STRING("ndefdiscovered"));
  LOG("DOM: Event dispatched (%d)", NS_ERROR_GET_CODE(rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Nfc::TagLost(const nsAString &aNfcHandle) {
  LOG("DOM: Received TagLost callback => create event");

  jsval result;
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  int length = aNfcHandle.Length();
  if (!length || !JS_ParseJSON(sc->GetNativeContext(), static_cast<const jschar*>(PromiseFlatString(aNfcHandle).get()),
       aNfcHandle.Length(), &result)) {
    LOG("DOM: Couldn't parse JSON");
    return NS_ERROR_UNEXPECTED;
  }

  // Dispatch incoming event.
  nsRefPtr<NfcNdefEvent> event = NfcNdefEvent::Create(result);
  NS_ASSERTION(event, "This should never fail!");

  LOG("DOM: Created event => dispatching");
  rv = event->Dispatch(ToIDOMEventTarget(), NS_LITERAL_STRING("taglost"));
  LOG("DOM: Event dispatched (%d)", NS_ERROR_GET_CODE(rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static JSBool
JSONCreator(const jschar* aBuf, uint32_t aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const PRUnichar*>(aBuf),
                 static_cast<PRUint32>(aLen));
  return true;
}

nsresult
NdefRecord_Validate(const jsval& aRecords, JSContext* aCx)
{
  JSObject &obj = aRecords.toObject();
  // Check if array
  if (!JS_IsArrayObject(aCx, &obj)) {
    LOG("error: WriteNdefTag requires an MozNdefRecord array.");
    return NS_ERROR_INVALID_ARG;
  }

  // Check length
  uint32_t length;
  if (!JS_GetArrayLength(aCx, &obj, &length)) {
    return NS_ERROR_FAILURE;
  }

  if (!length) {
    return NS_ERROR_FAILURE;
  }

  // Check object type (by name), (TODO: by signature)
  const char *ndefRecordName = "MozNdefRecord";
  for (uint32_t index = 0; index < length; index++) {
    jsval val;
    uint32_t namelen;
    if (JS_GetElement(aCx, &obj, index, &val)) {
      const char *name = JS_GetClass(JSVAL_TO_OBJECT(val))->name;
      namelen = strlen(name);
      if (strncmp(ndefRecordName, name, namelen)) {
        LOG("error: WriteNdefTag requires an MozNdefRecord array.");
        return NS_ERROR_INVALID_ARG;
      }
    }
  }
  return NS_OK;
}


// TODO, we want to take well formed object arrays.
NS_IMETHODIMP
Nfc::WriteNdefTag(const jsval& aRecords, JSContext* aCx, nsIDOMDOMRequest** aDomRequest_ret_val)
{
  nsresult rv;
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");

  // First parameter needs to be an array, and of type MozNdefRecord
  if (0 && !NdefRecord_Validate(aRecords, aCx)) {
    LOG("error: WriteNdefTag requires an MozNdefRecord array.");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  rv = rs->CreateRequest(GetOwner(), getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

  // Call to NFC.js
  nsIDOMDOMRequest *req = request.get();
  mNfc->WriteNdefTag(aRecords, req);
  NS_ASSERTION(aDomRequest_ret_val, "Null pointer?!");
  if (req) {
    request.forget(aDomRequest_ret_val);
  }

  return NS_OK;
}

// TODO: make private
NS_IMETHODIMP
Nfc::SendToNfcd(const nsAString& message)
{
  nsresult rv = mNfc->SendToNfcd(message);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
NS_NewNfc(nsPIDOMWindow* aWindow, nsIDOMNfc** aNfc)
{
  NS_ASSERTION(aWindow, "Null pointer!");

  // Make sure we're dealing with an inner window.
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
                               aWindow :
                               aWindow->GetCurrentInnerWindow();
  NS_ENSURE_TRUE(innerWindow, NS_ERROR_FAILURE);

  // Make sure we're being called from a window that we have permission to
  // access.
  if (!nsContentUtils::CanCallerAccess(innerWindow)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Need the document in order to make security decisions.
  nsCOMPtr<nsIDocument> document =
    do_QueryInterface(innerWindow->GetExtantDocument());
  NS_ENSURE_TRUE(document, NS_NOINTERFACE);

  // Do security checks. We assume that chrome is always allowed and we also
  // allow a single page specified by preferences.
  /*if (!nsContentUtils::IsSystemPrincipal(document->NodePrincipal())) {
    nsCOMPtr<nsIURI> originalURI;
    nsresult rv =
      document->NodePrincipal()->GetURI(getter_AddRefs(originalURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> documentURI;
    rv = originalURI->Clone(getter_AddRefs(documentURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // Strip the query string (if there is one) before comparing.
    nsCOMPtr<nsIURL> documentURL = do_QueryInterface(documentURI);
    if (documentURL) {
      rv = documentURL->SetQuery(EmptyCString());
      NS_ENSURE_SUCCESS(rv, rv);
    }

    bool allowed = false;

    // The pref may not exist but in that case we deny access just as we do if
    // the url doesn't match.
    nsCString whitelist;
    if (NS_SUCCEEDED(Preferences::GetCString(DOM_TELEPHONY_APP_PHONE_URL_PREF,
                                             &whitelist))) {
      nsCOMPtr<nsIIOService> ios = do_GetIOService();
      NS_ENSURE_TRUE(ios, NS_ERROR_FAILURE);

      nsCCharSeparatedTokenizer tokenizer(whitelist, ',');
      while (tokenizer.hasMoreTokens()) {
        nsCOMPtr<nsIURI> uri;
        if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), tokenizer.nextToken(),
                                   nsnull, nsnull, ios))) {
          rv = documentURI->EqualsExceptRef(uri, &allowed);
          NS_ENSURE_SUCCESS(rv, rv);

          if (allowed) {
            break;
          }
        }
      }
    }

    if (!allowed) {
      *aTelephony = nsnull;
      return NS_OK;
    }
  }*/

  // Security checks passed, make a NFC object.
  nsIInterfaceRequestor* ireq = mozilla::dom::gonk::SystemWorkerManager::GetInterfaceRequestor();
  NS_ENSURE_TRUE(ireq, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsINfc> nfc = do_GetInterface(ireq);
  NS_ENSURE_TRUE(nfc, NS_ERROR_UNEXPECTED);

  nsRefPtr<Nfc> domNfc = Nfc::Create(innerWindow, nfc);
  NS_ENSURE_TRUE(domNfc, NS_ERROR_UNEXPECTED);

  domNfc.forget(aNfc);
  return NS_OK;
}
