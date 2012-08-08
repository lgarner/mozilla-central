/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
"use strict";
 
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
 
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
 
const DEBUG = true; // set to true to see debug messages
 
const NFCCONTENTHELPER_CID =
  Components.ID("{4d72c120-da5f-11e1-9b23-0800200c9a66}");
 
const NFC_IPC_MSG_NAMES = [
  "NFC:NdefDiscovered",
  "NFC:TagLost",
  "NFC:NdefWriteStatus"
];
 
XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIFrameMessageManager");
 
function NfcContentHelper() {
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  this.initRequests();
  this.initMessageListener(NFC_IPC_MSG_NAMES);
}
 
NfcContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
 
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINfcContentHelper,
                                         Ci.nsIObserver]),
  classID:   NFCCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: NFCCONTENTHELPER_CID,
                                    classDescription: "NfcContentHelper",
                                    interfaces: [Ci.nsINfcContentHelper]}),
 
 
  sendToNfcd: function sendToNfcd(message) {
    cpmm.sendAsyncMessage("NFC:SendToNfcd", message);
  },

  writeNdefTag: function writeNdefTag(window, records) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));

    var encodedRecords = new Array();
    for(var i=0; i < records.length; i++) {
      var record = records[i];
      encodedRecords.push({ 
        tnf: record.tnf,
        type: btoa(record.type),
        id: btoa(record.id),
        payload: btoa(record.payload),
      });
    }

    cpmm.sendAsyncMessage("NFC:WriteNdefTag", {
      requestId: requestId,
      records: encodedRecords
    });
    return request;
  },
 
  _callbacks: null,
 
  registerCallback: function registerCallback(callback) {
    if (this._callbacks) {
      if (this._callbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._callbacks = [];
    }
    this._callbacks.push(callback);
    debug("Registered callback: " + callback);
  },
 
  unregisterCallback: function unregisterCallback(callback) {
    if (!this._callbacks) {
      return;
    }
    let index = this._callbacks.indexOf(callback);
    if (index != -1) {
      this._callbacks.splice(index, 1);
      debug("Unregistered callback: " + callback);
    }
  },
 
  receiveMessage: function receiveMessage(message) {
    let request;
    debug("Received message '" + message.name + "': " + JSON.stringify(message));
    switch (message.name) {
      case "NFC:NdefDiscovered":
        this.handleNdefDiscovered(message.json);
        break;
      case "NFC:TagLost":
        this.handleTagLost(message.json);
        break;
      case "NFC:NdefWriteStatus":
        this.handleNdefWriteStatus(message.json);
        break;
    }
  },
 
  handleNdefDiscovered: function handleNdefDiscovered(message) {
    let records = message.content.records;
    for (var i = 0; i < records.length; i++) {
      records[i].type = atob(records[i].type);
      records[i].id = atob(records[i].id);
      records[i].payload = atob(records[i].payload);
    }
    this._deliverCallback("ndefDiscovered", records)
  },

  handleTagLost: function handleTagLost(message) {
     this._deliverCallback("tagLost", message);
  },

  requestMap: null,

  handleNdefWriteStatus: function handleNdefWriteStatus(message) {
    let response = message.content; // Subfields of content: requestId, status, optional message
    debug("handleNdefWriteStatus (" + response.requestId + ", " + response.status + ")");
    let requestId = atob(response.requestId);
    if (response.status == "OK") {
      this.fireRequestSuccess(requestId, response);
    } else {
      this.fireRequestError(requestId, response);
    }

  },

  fireRequestSuccess: function fireRequestSuccess(requestId, result) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing success for id: " + requestId +
              ", result: " + JSON.stringify(result));
      }
      return;
    }

    if (DEBUG) {
      debug("fire request success, id: " + requestId +
            ", result: " + JSON.stringify(result));
    }
    Services.DOMRequest.fireSuccess(request, result);
  },

  fireRequestError: function fireRequestError(requestId, error) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing error for id: " + requestId +
              ", error: " + JSON.stringify(error));
      }
      return;
    }

    if (DEBUG) {
      debug("fire request error, id: " + requestId +
            ", result: " + JSON.stringify(error));
    }
    Services.DOMRequest.fireError(request, error);
  },
 
  _deliverCallback: function _deliverCallback(name, args) {
    // We need to worry about callback registration state mutations during the
    // callback firing. The behaviour we want is to *not* call any callbacks
    // that are added during the firing and to *not* call any callbacks that are
    // removed during the firing. To address this, we make a copy of the
    // callback list before dispatching and then double-check that each callback
    // is still registered before calling it.
    if (!this._callbacks) {
      return;
    }
    let callbacks = this._callbacks.slice();
    for each (let callback in callbacks) {
      if (this._callbacks.indexOf(callback) == -1) {
        continue;
      }
      let handler = callback[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        let str = JSON.stringify(args);
        debug("Delivering message \"" + str + "\" to " + callback[name] + " callback");
        handler.apply(callback, [ str ]);
      } catch (e) {
        debug("callback handler for " + name + " threw an exception: " + e);
      }
    }
  },
 
  // nsIObserver
 
  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      this.removeMessageListener();
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
    }
  }
 
};
 
const NSGetFactory = XPCOMUtils.generateNSGetFactory([NfcContentHelper]);
 
let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- NfcContentHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
