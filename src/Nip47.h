#ifndef _NIP47_H
#define _NIP47_H
#include <Arduino.h>
#include "ArduinoJson.h"
#include "Nip04.h"
#include "NostrEvent.h"
#include "NostrString.h"
#include "NostrUtils.h"
#include "NostrPool.h"
#include <initializer_list>
#include <vector>
namespace nostr {



struct PaymentReceivedParams {
    unsigned long long amount;
    NostrString paymentHash;
    NostrString sender;

    PaymentReceivedParams() : amount(0), paymentHash(""), sender("") {}
    PaymentReceivedParams(const PaymentReceivedParams& other) 
        : amount(other.amount), paymentHash(other.paymentHash), sender(other.sender) {}
    ~PaymentReceivedParams() {}
};

struct PaymentSentParams {
    unsigned long long amount;
    NostrString paymentHash;
    NostrString recipient;

    PaymentSentParams() : amount(0), paymentHash(""), recipient("") {}
    PaymentSentParams(const PaymentSentParams& other) 
        : amount(other.amount), paymentHash(other.paymentHash), recipient(other.recipient) {}
    ~PaymentSentParams() {}
};

struct Nip47Notification {
    NostrString method;
    union {
        PaymentReceivedParams paymentReceived;
        PaymentSentParams paymentSent;
    };
    enum class ActiveMember { None, PaymentReceived, PaymentSent } active;

    // Default constructor: Initializes with no active member
    Nip47Notification() : method(""), active(ActiveMember::None) {
        // Explicitly initialize paymentReceived to avoid undefined state
        new (&paymentReceived) PaymentReceivedParams();
    }

    // Constructor for payment_received
    Nip47Notification(const NostrString& m, const PaymentReceivedParams& pr) 
        : method(m), active(ActiveMember::PaymentReceived) {
        new (&paymentReceived) PaymentReceivedParams(pr);
    }

    // Constructor for payment_sent
    Nip47Notification(const NostrString& m, const PaymentSentParams& ps) 
        : method(m), active(ActiveMember::PaymentSent) {
        new (&paymentSent) PaymentSentParams(ps);
    }

    // Copy constructor
    Nip47Notification(const Nip47Notification& other) : method(other.method), active(other.active) {
        switch (active) {
            case ActiveMember::PaymentReceived:
                new (&paymentReceived) PaymentReceivedParams(other.paymentReceived);
                break;
            case ActiveMember::PaymentSent:
                new (&paymentSent) PaymentSentParams(other.paymentSent);
                break;
            case ActiveMember::None:
                new (&paymentReceived) PaymentReceivedParams(); // Default state
                break;
        }
    }

    // Destructor
    ~Nip47Notification() {
        switch (active) {
            case ActiveMember::PaymentReceived:
                paymentReceived.~PaymentReceivedParams();
                break;
            case ActiveMember::PaymentSent:
                paymentSent.~PaymentSentParams();
                break;
            case ActiveMember::None:
                paymentReceived.~PaymentReceivedParams(); // Clean up default state
                break;
        }
    }

    // Assignment operator
    Nip47Notification& operator=(const Nip47Notification& other) {
        if (this != &other) {
            this->~Nip47Notification(); // Destroy current state
            method = other.method;
            active = other.active;
            switch (active) {
                case ActiveMember::PaymentReceived:
                    new (&paymentReceived) PaymentReceivedParams(other.paymentReceived);
                    break;
                case ActiveMember::PaymentSent:
                    new (&paymentSent) PaymentSentParams(other.paymentSent);
                    break;
                case ActiveMember::None:
                    new (&paymentReceived) PaymentReceivedParams();
                    break;
            }
        }
        return *this;
    }
};

typedef struct s_Invoice {
    NostrString invoice;
    unsigned long amount;
} Invoice;

typedef struct s_TLVRecords {
    NostrString type;
    NostrString value;
} TLVRecords;

typedef struct s_KeySend {
    NostrString pubkey;
    unsigned long amount;
    NostrString preimage;
    std::initializer_list<TLVRecords> tlv;
} KeySend;

template <typename T> class Nip47Response {
  public:
    Nip47Response() {}

    Nip47Response(NostrString errorCode, NostrString errorMessage, NostrString resultType, T result) : errorCode(errorCode), errorMessage(errorMessage), resultType(resultType), result(result) {}
    NostrString errorCode = "";
    NostrString errorMessage = "";
    NostrString resultType = "";
    T result;
};

typedef struct s_PayInvoiceResponse {
    NostrString preimage;
} PayInvoiceResponse;

typedef struct s_MultiPayInvoiceResponse {
    NostrString preimage;
    NostrString d;
} MultiPayInvoiceResponse;

typedef struct s_PayKeySendResponse {
    NostrString preimage;
} PayKeySendResponse;

typedef struct s_MultiPayKeySendResponse {
    NostrString preimage;
    NostrString d;

} MultiPayKeySendResponse;

typedef struct s_MakeInvoiceResponse {
    NostrString type;
    NostrString invoice;
    NostrString description;
    NostrString descriptionHash;
    NostrString preimage;
    NostrString paymentHash;
    unsigned long long amount;
    unsigned long long feesPaid;
    unsigned long long createdAt;
    unsigned long long expiresAt;
    JsonObject metadata;
} MakeInvoiceResponse;

typedef struct s_LookUpInvoiceResponse {
    NostrString type;
    NostrString invoice;
    NostrString description;
    NostrString descriptionHash;
    NostrString preimage;
    NostrString paymentHash;
    unsigned long long amount;
    unsigned long long feesPaid;
    unsigned long long createdAt;
    unsigned long long expiresAt;
    unsigned long long settledAt;

    JsonObject metadata;
} LookUpInvoiceResponse;

typedef struct s_Transaction {
    NostrString type;
    NostrString invoice;
    NostrString description;
    NostrString descriptionHash;
    NostrString preimage;
    NostrString paymentHash;
    unsigned long long amount;
    unsigned long long feesPaid;
    unsigned long long createdAt;
    unsigned long long expiresAt;
    unsigned long long settledAt;

    JsonObject metadata;

} Transaction;

typedef struct s_ListTransactionsResponse {
    std::vector<Transaction> transactions;
} ListTransactionsResponse;

typedef struct s_GetBalanceResponse {
    unsigned long long balance;
} GetBalanceResponse;

typedef struct s_GetInfoResponse {
    NostrString alias;
    NostrString color;
    NostrString pubkey;
    NostrString network;
    unsigned long long blockHeight;
    NostrString blockHash;
    std::vector<NostrString> methods;
    std::vector<NostrString> notifications;
} GetInfoResponse;

typedef struct s_NWCData {
    NostrString relay;
    NostrString pubkey;
    NostrString secret;
} NWCData;

class Nip47 {
  public:
    Nip47(){};
    Nip47(Nip04 nip04, NostrString servicePubKey, NostrString userPrivKey) : servicePubKey(servicePubKey), userPrivKey(userPrivKey), nip04(nip04) {}
    SignedNostrEvent payInvoice(NostrString invoice, unsigned long amount = -1);
    SignedNostrEvent multiPayInvoice(std::initializer_list<Invoice> invoices);
    SignedNostrEvent payKeySend(NostrString pubkey, unsigned long amount, NostrString preimage = "", std::initializer_list<TLVRecords> tlv = {});
    SignedNostrEvent multiPayKeySend(std::initializer_list<KeySend> keySends);
    SignedNostrEvent makeInvoice(unsigned long amount, NostrString description = "", NostrString descriptionHash = "", unsigned long expiry = 0);
    SignedNostrEvent lookUpPaymentHash(NostrString paymentHash);
    SignedNostrEvent lookUpInvoice(NostrString invoice);
    SignedNostrEvent listTransactions(unsigned long from = 0, unsigned long until = 0, int limit = 0, int offset = 0, bool unpaid = false, NostrString type = "");
    SignedNostrEvent getBalance();
    SignedNostrEvent getInfo();

    void parseResponse(SignedNostrEvent *response, Nip47Response<PayInvoiceResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<MultiPayInvoiceResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<PayKeySendResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<MultiPayKeySendResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<MakeInvoiceResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<LookUpInvoiceResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<ListTransactionsResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<GetBalanceResponse> &out);
    void parseResponse(SignedNostrEvent *response, Nip47Response<GetInfoResponse> &out);
    void subscribeToNotifications(NostrPool& pool, NostrString userPubKey, std::function<void(Nip47Notification)> onNotification);
    void handleNotification(SignedNostrEvent* event);
    static void parseNWC(NostrString, NWCData &);

  private:
    NostrString servicePubKey;
    NostrString userPrivKey;
    Nip04 nip04;
    SignedNostrEvent createEvent(NostrString method, JsonDocument doc);
    std::function<void(Nip47Notification)> notificationCallback;
};
} 

#endif 