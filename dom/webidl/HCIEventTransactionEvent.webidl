/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright © 2014 Deutsche Telekom, Inc.
*/

[Constructor(DOMString type, optional HCIEventTransactionEventInit eventInitDict)]
interface HCIEventTransactionEvent: Event
{
  readonly attribute Uint8Array? aid;
  readonly attribute Uint8Array payload;
  readonly attribute DOMString origin;
};

dictionary HCIEventTransactionEventInit: EventInit
{
  Uint8Array? aid = null;
  required Uint8Array payload;
  DOMString origin = "";
};
