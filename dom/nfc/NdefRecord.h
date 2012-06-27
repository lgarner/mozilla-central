
#include "nsIDOMNdefRecord.h"
#include "nsIDOMNode.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsString.h"


/**
 * When we send a mozbrowseropenwindow event (an instance of CustomEvent), we
 * use an instance of this class as the event's detail.
 */
class NdefRecord : public nsIDOMNdefRecord
{
public:
  NS_DECL_ISUPPORTS
 
  NdefRecord(const char& tnf,
               const nsAString& type,
               const nsAString& id,
               const nsAString& payload)
    : tnf(tnf)
    , type(type)
    , id(id)
    , payload(payload)
  {}

  virtual ~NdefRecord() {}

  const char tnf;
  const nsString type;
  const nsString id;
  const nsString payload;
};
