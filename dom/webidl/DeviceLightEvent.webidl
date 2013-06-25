/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional DeviceLightEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface DeviceLightEvent : Event
{
  readonly attribute double value;
};

dictionary DeviceLightEventInit : EventInit
{
  unrestricted double value = Infinity;
};
