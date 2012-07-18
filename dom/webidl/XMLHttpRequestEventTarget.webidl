/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * www.w3.org/TR/2012/WD-XMLHttpRequest-20120117/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[NoInterfaceObject]
interface XMLHttpRequestEventTarget : EventTarget {
  // event handlers
  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onloadstart;

  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onprogress;

  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onabort;

  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onerror;

  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onload;

  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? ontimeout;

  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onloadend;
};
