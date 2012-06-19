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

#ifndef mozilla_dom_nfc_nfc_h__
#define mozilla_dom_nfc_nfc_h__

#include "NfcCommon.h"

#include "nsIDOMNfc.h"
#include "nsINfc.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_NFC_NAMESPACE

class Nfc : public nsDOMEventTargetHelper,
                  public nsIDOMNfc
{
  nsCOMPtr<nsINfc> mNfc;
  nsCOMPtr<nsINFCCallback> mNFCCallback;

  NS_DECL_EVENT_HANDLER(ndefdiscovered)

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMNFC
  NS_DECL_NSINFCCALLBACK
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                   Nfc,
                                                   nsDOMEventTargetHelper)

  static already_AddRefed<Nfc>
  Create(nsPIDOMWindow* aOwner, nsINfc* aRIL);

  nsIDOMEventTarget*
  ToIDOMEventTarget() const
  {
    return static_cast<nsDOMEventTargetHelper*>(
             const_cast<Nfc*>(this));
  }

  nsISupports*
  ToISupports() const
  {
    return ToIDOMEventTarget();
  }

  nsINfc*
  NFC() const
  {
    return mNfc;
  }

private:
  Nfc();
  ~Nfc();

  class NFCCallback : public nsINFCCallback
  {
    Nfc* mNfc;

  public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_NSINFCCALLBACK(mNfc->)

    NFCCallback(Nfc* aNfc)
    : mNfc(aNfc)
    {
      NS_ASSERTION(mNfc, "Null pointer!");
    }
  };
};

END_NFC_NAMESPACE

#endif // mozilla_dom_nfc_nfc_h__
