/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2014, Deutsche Telekom, Inc. */

"use strict";

const DEBUG = true;
function debug(s) {
  if (DEBUG) dump("-*- Nfc DOM: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * MozNDEFRecord
 */
function MozNDEFRecord() {
  debug("In MozNDEFRecord Constructor");
}
MozNDEFRecord.prototype = {
  _window: null,

  _tnf: 0x0,
  _type: null,
  _id: null,
  _payload: null,

  // nsIDOMGlobalPropertyInitializer
  init: function nr_init(aWindow) {
    debug("MozNDEFRecord: init()");
    this._window = aWindow;
  },

  __init: function nr___init(aTnf, aType, aId, aPayload) {
   debug("MozNDEFRecord: __init()");
   this._tnf = aTnf;
   this._type = aType;
   this._id = aId;
   this._payload = aPayload;
  },

  // nsISystemMessagesWrapper...but this is not a message.
  wrapMessage: function nr_wrapMessage(aMessage, aWindow) {
    //return Cu.cloneInto(this, this._window);
  }, 

  initialize: function nr_initialize(aWindow, tnf, type, id, payload) {
    debug("MozNDEFRecord: initialize()"); 
    this._window = aWindow;

    this.tnf = tnf;
    this.type = type;
    this.id = id;
    this.payload = payload;
  },

  get tnf() {
    return this._tnf;
  },

  get type() {
    return this._type;
  },

  get id() {
    return this._id;
  },

  get payload() {
    return this._payload;
  },
  
  doSomething: function nr_doSomething() {
    debug("doSomething");
  },

  classID: Components.ID("{b0687a80-11fe-11e4-9191-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/NDEFRecord;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISystemMessagesWrapper]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozNDEFRecord]);
