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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Sinker Li <thinker@codemud.net>
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

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var NFC = {};
Cu.import("resource://gre/modules/nfc_consts.js", NFC);

const DEBUG = true; // set to true to see debug messages

const NFC_CID =
  Components.ID("{2ff24790-5e74-11e1-b86c-0800200c9a66}");

function Nfc() {
  this.worker = new ChromeWorker("resource://gre/modules/nfc_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);
  debug("Starting Worker\n");
  this.currentState = {
  };
}
Nfc.prototype = {

  classID:   NFC_CID,
  classInfo: XPCOMUtils.generateCI({classID: NFC_CID,
                                    classDescription: "Nfc",
                                    interfaces: [Ci.nsIWorkerHolder,
                                                 Ci.nsINfc]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsINfc]),

  onerror: function onerror(event) {
    debug("Got an error: " + event.filename + ":" +
          event.lineno + ": " + event.message + "\n");
    event.preventDefault();
  },

  /**
   * Process the incoming message from the NFC worker
   */
  onmessage: function onmessage(event) {
    let message = event.data;
    debug("Received message: " + JSON.stringify(message));
    switch (message.type) {
      case "ndefDiscovered":
        this.handleNdefDiscovered(message);
        break;
      case "tagLost":
        this.handleTagLost(message);
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  },

  /**
   * Handle data call list.
   */
  handleNdefDiscovered: function handleNdefDiscovered(message) {
    let records = message.content.records;
    for (var i = 0; i < records.length; i++) {
      records[i].type = atob(records[i].type);
      records[i].id = atob(records[i].id);
      records[i].payload = atob(records[i].payload);
    }
    this._deliverCallback("ndefDiscovered",
                          records)
  },

  handleTagLost: function handleTagLost(handle) {
     this._deliverCallback("tagLost", handle);
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


  // nsINfcWorker

  worker: null,

  // nsINfc

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

  writeNdefTag: function writeNdefTag(arrayRecords, requestId) {
    var jsonStr;
    var type;
    var id;
    var payload;
    var i;
  
    debug("XXXXXXXXXXXXXXXXXX Here! XXXXXXXXXXXXXXXXXXXXXXXXX");
    // Begin Hack: XPCOM object MozNdefRecord doesn't Stringify. Encode and JSONify an XPCOM object by parts...
    jsonStr = "[";
    debug("----  Record " + arrayRecords + " Length: " + arrayRecords.length + "----");
    for (i = 0; i < arrayRecords.length; i++) {
      type = btoa(arrayRecords[i].type);
      id = btoa(arrayRecords[i].id);
      payload = btoa(arrayRecords[i].payload);
      jsonStr += '{' + ' "tnf": "' + arrayRecords[i].tnf + '", "type": "' + type + '", "id": "' + id + '", "payload" :"' + payload + '"}';
      if (i+1 != arrayRecords.length) {
        jsonStr += ", "; 
      }
      debug("----  item [" + i + "]   ----");
    }
    jsonStr += "]";
    var prefix = '{ "type": "ndefDiscovered", "content": { "records": ';
    var suffix = " }}";
    var outMessage = prefix + jsonStr + suffix;
    debug("Outgoing NDef post message done here" + outMessage);
    // End hack

    this.worker.postMessage({type: "writeNdefTag", content: outMessage});
  },

  sendToNfcd: function sendToNfcd(message) {
    this.worker.postMessage({type: "directMessage", content: message});
  }

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([Nfc]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- Nfc: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
