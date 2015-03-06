"use strict";

/* globals console, debug, SEUtils, GPAccessRulesManager */
/* exported Components */

var Components = {
  classes: {
    "@mozilla.org/secureelement/access-control/rules-manager/gp;1": {
      getService: () => { return new GPAccessRulesManager(); }
    }
  },
  interfaces: {
    nsIAccessRulesManager: {
      ALLOW_ALL: "allowed-all",
      DENY_ALL: "denied-all",
      ALL_APPLET: "all"
    }
  },
  utils: {
    import: function() {
      debug("Components.utils.import" + arguments);
    }
  },
  results: {},
  ID: () => {}
};

var MockParentProcessMessageManager = {
  addMessageListener: function() {
    debug("MockParentProcessMessageManager.addMessageListener" + arguments);
  }
};

var MockRilContentHelper = {
  iccOpenChannel: function(clientId, aid, callback) {
    debug("Opening channel: " + aid);
    let channelId = 1;
    this.mockPosition = 0;
    callback.notifyOpenChannelSuccess(channelId);
  },

  iccExchangeAPDU: function(clientId, channel, cla, ins, p1, p2, lc, data,
                            callback) {
    var prettify = function(str) {
      return str.replace(/\s+/g, "")
                .replace(/(..)/g, "$1 ")
                .replace(/\s$/, "")
                .toUpperCase();
    };

    var sanitize = function(str) {
      return str.replace(/\s+/g, "")
                .toUpperCase();
    };

    let scenario = window.ACE_TEST_SCENARIO;

    if (!scenario) {
      debug("No scenario '" + scenario + "' found.");
      return;
    }

    let mock = scenario.steps;

    debug("Mock command #" + this.mockPosition);
    debug("cla: " + cla + ", ins: " + ins + ", p1: " + p1 + ", p2: " +
          p2 + ", lc: " + lc + " data: " +  data);
    let apduBytes = [cla, ins, p1, p2, lc];
    if (data) {
      apduBytes = apduBytes.concat(SEUtils.hexStringToByteArray(data));
    }

    let command = mock[this.mockPosition];
    let request = prettify(SEUtils.byteArrayToHexString(apduBytes));

    if (!command) {
      callback.notifyExchangeAPDUResponse(0x6A, 0x82, null);
      return;
    }

    if (command.request !== request) {
      debug("Mock " + command.desc + ": invalid request: " + request +
            " at position " + this.mockPosition);
      return callback.notifyExchangeAPDUResponse(0x6A, 0x82, null);
    }

    this.mockPosition += 1;
    let response = sanitize(command.response);
    callback.notifyExchangeAPDUResponse(0x90, 0x00, response);
  },

  iccCloseChannel: function(clientId, channel, callback) {
    callback.notifyCloseChannelSuccess(channel);
  },
};

var MockUiccConnector = {
  openChannel: function(aid, cbk) {
    debug("Opening channel: " + aid);
    let channelId = 1;
    this.mockPosition = 0;
    cbk.notifyOpenChannelSuccess(channelId, "mock open response");
  },

  closeChannel: function(channelId, cbk) {
    cbk.notifyCloseChannelSuccess();
  },

  exchangeAPDU: function(channelId, cla, ins, p1, p2, data, lc, cbk) {
    var prettify = function(str) {
      return str.replace(/\s+/g, "")
                .replace(/(..)/g, "$1 ")
                .replace(/\s$/, "")
                .toUpperCase();
    };

    var sanitize = function(str) {
      return str.replace(/\s+/g, "")
                .toUpperCase();
    };

    let scenario = window.ACE_TEST_SCENARIO;

    if (!scenario) {
      debug("No scenario '" + scenario + "' found.");
      return;
    }

    let mock = scenario.steps;

    debug("Mock command #" + this.mockPosition);
    debug("cla: " + cla + ", ins: " + ins + ", p1: " + p1 + ", p2: " +
          p2 + ", lc: " + lc + " data: " +  data);
    let apduBytes = [cla, ins, p1, p2, lc];
    if (data) {
      apduBytes = apduBytes.concat(SEUtils.hexStringToByteArray(data));
    }

    let command = mock[this.mockPosition];
    let request = prettify(SEUtils.byteArrayToHexString(apduBytes));

    if (!command) {
      console.log("No such command", command);
      cbk.notifyExchangeAPDUResponse(0x6A, 0x82, null);
      return;
    }

    // todo: uncomment
    // if (command.request !== request) {
    //   debug("Mock " + command.desc + ": invalid request: " + request +
    //         " at position " + this.mockPosition);
    //   return cbk.notifyExchangeAPDUResponse(0x6A, 0x82, null);
    // }

    this.mockPosition += 1;
    let response = sanitize(command.response);
    cbk.notifyExchangeAPDUResponse(0x90, 0x00, response);
  }
};

var MockWebapps = {
  getAppByManifestURL: function() {
    debug("MockWebapps.getAppByManifestURL" + arguments);
  }
};

var addMockProperty = function(target, name, contractId, interfaceName) {
  var mockServices = {
    "@mozilla.org/parentprocessmessagemanager;1 -- nsIMessageListenerManager":
      MockParentProcessMessageManager,
    "@mozilla.org/ril/content-helper;1 -- nsIIccProvider": MockRilContentHelper,
    "@mozilla.org/secureelement/connector/uicc;1 -- nsISecureElementConnector": MockUiccConnector
  };

  var mockModules = {
    "resource://gre/modules/Webapps.jsm": MockWebapps,
    "resource://gre/modules/SEUtils.jsm": SEUtils,
  };

  var impl = null;
  if (arguments.length === 3) {
    impl = mockModules[contractId];
  } else {
    var id = contractId + " -- " + interfaceName;
    impl = mockServices[id];
  }

  if (!impl) {
    debug("No mock implementation found for " + contractId + "@" +
          interfaceName);
    return;
  }

  debug("Mock implementation for " + contractId + "@"  + interfaceName +
        " attached as " + name);
  target[name] = impl;
};

window.XPCOMUtils = {
  defineLazyServiceGetter: addMockProperty,
  defineLazyModuleGetter: addMockProperty,
  defineLazyGetter: () => {},
  generateQI: () => {},
  generateNSGetFactory: () => {}
};

window.ppmm = {
  addMessageListener: function() {
    debug("ppmm.addMessageListener " + arguments);
  }
};

window.dump = function(str) {
  console.log(str);
};

window.libcutils = {
  property_get: function() {}
};

window.SE = {
  TYPE_UICC: "uicc"
};
