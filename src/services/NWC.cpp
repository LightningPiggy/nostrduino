#include "NWC.h"
#include "Nip47.h"

const char* NWC_RESPONSE_KIND = "23195";
const char* NWC_NOTIFICATION_KIND = "23196";

using namespace nostr;

NWC::~NWC() {
    this->close();
}

NWC::NWC(Transport *transport, NostrString nwcUrl) {
    this->pool = std::unique_ptr<NostrPool>(new NostrPool(transport));
    Nip47::parseNWC(nwcUrl, this->nwc);

    Utils::log("NWC RELAY: " + this->nwc.relay);
    Utils::log("NWC PUBKEY: " + this->nwc.pubkey);
    Utils::log("NWC PRIVKEY: ******");

    NostrString accountSecret = this->nwc.secret;

    this->nip47 = Nip47(Nip04(), this->nwc.pubkey, accountSecret);
    this->accountPubKey = Utils::getPublicKey(accountSecret);
}

void NWC::close() {
    this->pool->close();
    this->callbacks.clear();
}

void NWC::loop() {
    this->pool->loop();
    for (auto it = this->callbacks.begin(); it != this->callbacks.end();) {
        if (it->get()->n == 0) {
            NostrString subId = it->get()->subId;
            this->pool->closeSubscription(subId);
            it = this->callbacks.erase(it);
            continue;
        }
        if (Utils::unixTimeSeconds() - it->get()->timestampSeconds > it->get()->timeoutSeconds) {
            NostrString subId = it->get()->subId;
            this->pool->closeSubscription(subId);
            it->get()->onErr("OTHER", "timeout");
            it = this->callbacks.erase(it);
        } else {
            it++;
        }
    }
}

// Subscribe to NWC reponses or notifications, and send an event, if provided.
NostrString NWC::sendEvent(SignedNostrEvent *event = nullptr) {
    const char* kind = event ? NWC_RESPONSE_KIND : NWC_NOTIFICATION_KIND; // If an event is sent, expect a reponse. Otherwise, expect a notification.
    std::map<NostrString, std::initializer_list<NostrString>> filter = {
        {"kinds", {kind}},
        {"#p", {this->accountPubKey}}
    };
    if (event) {
        filter["#e"] = {event->getId()};
    }

    // Common subscription logic
    NostrString subId = this->pool->subscribeMany(
        {this->nwc.relay}, {{filter}},
        [&](const NostrString &subId, SignedNostrEvent *receivedEvent) {
            NostrString ref = event ? receivedEvent->getTags()->getTag("e")[0] : receivedEvent->getSubId();
            for (auto it = this->callbacks.begin(); it != this->callbacks.end(); it++) {
                if (NostrString_equals(event ? it->get()->eventId : it->get()->subId, ref)) {
                    if (!event || it->get()->n > 0) { // Check n only for event case, because 
                        it->get()->call(&this->nip47, receivedEvent);
                        if (event) it->get()->n--;
                    }
                    break;
                }
            }
        },
        [&](const String &subId, const String &reason) { Utils::log("NWC: closed subscription: " + reason); },
        [&](const String &subId) { Utils::log("NWC: EOS"); }
    );

    // Publish if event is provided
    if (event) {
        this->pool->publish(
            {this->nwc.relay}, event,
            [&](const NostrString &eventId, bool status, const NostrString &msg) {
                Utils::log(status ? "NWC: event sent: " + eventId : "NWC: error sending event: " + msg);
            }
        );
    }

    return subId;
}

void NWC::subscribeNotifications(std::function<void(NotificationResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    std::unique_ptr<NWCResponseCallback<NotificationResponse>> callback(new NWCResponseCallback<NotificationResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->timeoutSeconds = NWC_INFINITE_TIMEOUT;
    callback->subId = sendEvent();
    callback->n = 1;
    this->callbacks.push_back(std::move(callback));
}

void NWC::payInvoice(NostrString invoice, unsigned long amount, std::function<void(PayInvoiceResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.payInvoice(invoice, amount);
    std::unique_ptr<NWCResponseCallback<PayInvoiceResponse>> callback(new NWCResponseCallback<PayInvoiceResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::multiPayInvoice(std::initializer_list<Invoice> invoices, std::function<void(MultiPayInvoiceResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.multiPayInvoice(invoices);
    std::unique_ptr<NWCResponseCallback<MultiPayInvoiceResponse>> callback(new NWCResponseCallback<MultiPayInvoiceResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = invoices.size();
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::payKeySend(NostrString pubkey, unsigned long amount, NostrString preimage, std::initializer_list<TLVRecords> tlv, std::function<void(PayKeySendResponse)> onRes,
                     std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.payKeySend(pubkey, amount, preimage, tlv);
    std::unique_ptr<NWCResponseCallback<PayKeySendResponse>> callback(new NWCResponseCallback<PayKeySendResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::multiPayKeySend(std::initializer_list<KeySend> keySends, std::function<void(MultiPayKeySendResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.multiPayKeySend(keySends);
    std::unique_ptr<NWCResponseCallback<MultiPayKeySendResponse>> callback(new NWCResponseCallback<MultiPayKeySendResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = keySends.size();
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::makeInvoice(unsigned long amount, NostrString description, NostrString descriptionHash, unsigned long expiry, std::function<void(MakeInvoiceResponse)> onRes,
                      std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.makeInvoice(amount, description, descriptionHash, expiry);
    std::unique_ptr<NWCResponseCallback<MakeInvoiceResponse>> callback(new NWCResponseCallback<MakeInvoiceResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::lookUpPaymentHash(NostrString paymentHash, std::function<void(LookUpInvoiceResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.lookUpPaymentHash(paymentHash);
    std::unique_ptr<NWCResponseCallback<LookUpInvoiceResponse>> callback(new NWCResponseCallback<LookUpInvoiceResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::lookUpInvoice(NostrString invoice, std::function<void(LookUpInvoiceResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.lookUpInvoice(invoice);
    std::unique_ptr<NWCResponseCallback<LookUpInvoiceResponse>> callback(new NWCResponseCallback<LookUpInvoiceResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::listTransactions(unsigned long from, unsigned long until, int limit, int offset, bool unpaid, NostrString type, std::function<void(ListTransactionsResponse)> onRes,
                           std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.listTransactions(from, until, limit, offset, unpaid, type);
    std::unique_ptr<NWCResponseCallback<ListTransactionsResponse>> callback(new NWCResponseCallback<ListTransactionsResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::getBalance(std::function<void(GetBalanceResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.getBalance();
    std::unique_ptr<NWCResponseCallback<GetBalanceResponse>> callback(new NWCResponseCallback<GetBalanceResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}

void NWC::getInfo(std::function<void(GetInfoResponse)> onRes, std::function<void(NostrString, NostrString)> onErr) {
    SignedNostrEvent ev = this->nip47.getInfo();
    std::unique_ptr<NWCResponseCallback<GetInfoResponse>> callback(new NWCResponseCallback<GetInfoResponse>());
    callback->onRes = onRes;
    callback->onErr = onErr;
    callback->timestampSeconds = Utils::unixTimeSeconds();
    callback->eventId = ev.getId();
    callback->n = 1;
    callback->subId = this->sendEvent(&ev);
    this->callbacks.push_back(std::move(callback));
}
