// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "core_io.h"
#include "clientversion.h"
#include "init.h"
#include "main.h"
#include "stake.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "script/standard.h"
#include "spork.h"
#include "timedata.h"
#include "txdb.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#include "walletdb.h"
#endif
#include "lux/storagecontroller.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "univalue/univalue.h"
#include "rpcutil.h"
#include "util.h"

using namespace boost;
using namespace boost::assign;
using namespace std;

/**
 * @note Do not add or change anything in the information returned by this
 * method. `getinfo` exists for backwards-compatibility only. It combines
 * information from wildly different sources in the program, which is a mess,
 * and is thus planned to be deprecated eventually.
 *
 * Based on the source of the information, new information should be added to:
 * - `getblockchaininfo`,
 * - `getnetworkinfo` or
 * - `getwalletinfo`
 *
 * Or alternatively, create a specific query method for the information.
 **/

UniValue getinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "Returns an object containing various state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,           (numeric) the server version\n"
            "  \"protocolversion\": xxxxx,   (numeric) the protocol version\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total lux balance of the wallet\n"
            "  \"darksend_balance\": xxxxxx, (numeric) the anonymized lux balance of the wallet\n"
            "  \"blocks\": xxxxxx,           (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,        (numeric) the time offset\n"
            "  \"connections\": xxxxx,       (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",     (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,       (numeric) the current difficulty\n"
            "  \"testnet\": true|false,      (boolean) if the server is using testnet or not\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee set in lux/kB\n"
            "  \"relayfee\": x.xxxx,         (numeric) minimum relay fee for non-free transactions in lux/kB\n"
            "  \"staking\": true|false,      (boolean) if the wallet is staking or not\n"
            "  \"errors\": \"...\"           (string) any error messages\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getinfo", "") + HelpExampleRpc("getinfo", ""));

    LOCK(cs_main);

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    UniValue obj(UniValue::VOBJ);

    UniValue diff(UniValue::VOBJ);
    CBlockIndex* powTip = GetLastBlockOfType(0);
    CBlockIndex* posTip = GetLastBlockOfType(1);

    obj.push_back(Pair("version", CLIENT_VERSION));
    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
        obj.push_back(Pair("balance", ValueFromAmount(pwalletMain->GetBalance())));
    }
#endif
    obj.push_back(Pair("blocks", (int)chainActive.Height()));
    obj.push_back(Pair("timeoffset", GetTimeOffset()));
    obj.push_back(Pair("connections", (int)vNodes.size()));
    obj.push_back(Pair("proxy", (proxy.IsValid() ? proxy.ToStringIPPort() : string())));

    diff.push_back(Pair("proof-of-work", (double)GetDifficulty(powTip)));
    diff.push_back(Pair("proof-of-stake", (double)GetDifficulty(posTip)));
    obj.push_back(Pair("difficulty", diff));

    obj.push_back(Pair("testnet", Params().NetworkIDString() != "main"));
    obj.push_back(Pair("networkactive", fNetworkActive));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
        obj.push_back(Pair("keypoolsize", (int)pwalletMain->GetKeyPoolSize()));
    }
    if (pwalletMain && pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee", ValueFromAmount(payTxFee.GetFeePerK())));
#endif
    obj.push_back(Pair("relayfee", ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    obj.push_back(Pair("staking", stake->IsActive()));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));
    return obj;
}

UniValue getstateinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getstateinfo\n"
                "\nReturns an object containing state info.\n"
                "\nResult:\n"
                "{\n"
                "  \"maxblocksize\": xxxxx,    (numeric) current maximum block size\n"
                "  \"blockgaslimit\": xxxxx,   (numeric) current block gas limit\n"
                "  \"mingasprice\": xxxxx,     (numeric) current minimum gas price\n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli("getstateinfo", "")
        );


    LOCK(cs_main);

    LuxDGP luxDGP(globalState.get());

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("maxblocksize", (int)luxDGP.getBlockSize(chainActive.Height())));
    obj.push_back(Pair("blockgaslimit", (int)luxDGP.getMinGasPrice(chainActive.Height())));
    obj.push_back(Pair("mingasprice", (int)luxDGP.getBlockGasLimit(chainActive.Height())));

    return obj;
}

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
private:
    isminetype mine;

public:
    DescribeAddressVisitor(isminetype mineIn) : mine(mineIn) {}

    void ProcessSubScript(const CScript& subscript, UniValue& obj, bool include_addresses = false) const
    {
        // Always present: script type and redeemscript
        txnouttype which_type;
        std::vector<std::vector<unsigned char>> solutions_data;
        Solver(subscript, which_type, solutions_data);
        obj.pushKV("script", GetTxnOutputType(which_type));
        obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));

        CTxDestination embedded;
        UniValue a(UniValue::VARR);
        if (ExtractDestination(subscript, embedded)) {
            // Only when the script corresponds to an address.
            UniValue subobj = boost::apply_visitor(*this, embedded);
            subobj.pushKV("address", EncodeDestination(embedded));
            subobj.pushKV("scriptPubKey", HexStr(subscript.begin(), subscript.end()));
            // Always report the pubkey at the top level, so that `getnewaddress()['pubkey']` always works.
            if (subobj.exists("pubkey")) obj.pushKV("pubkey", subobj["pubkey"]);
            obj.pushKV("embedded", std::move(subobj));
            if (include_addresses) a.push_back(EncodeDestination(embedded));
        } else if (which_type == TX_MULTISIG) {
            // Also report some information on multisig scripts (which do not have a corresponding address).
            // TODO: abstract out the common functionality between this logic and ExtractDestinations.
            obj.pushKV("sigsrequired", solutions_data[0][0]);
            UniValue pubkeys(UniValue::VARR);
            for (size_t i = 1; i < solutions_data.size() - 1; ++i) {
                CPubKey key(solutions_data[i].begin(), solutions_data[i].end());
                if (include_addresses) a.push_back(EncodeDestination(key.GetID()));
                pubkeys.push_back(HexStr(key.begin(), key.end()));
            }
            obj.pushKV("pubkeys", std::move(pubkeys));
        }

        // The "addresses" field is confusing because it refers to public keys using their P2PKH address.
        // For that reason, only add the 'addresses' field when needed for backward compatibility. New applications
        // can use the 'embedded'->'address' field for P2SH or P2WSH wrapped addresses, and 'pubkeys' for
        // inspecting multisig participants.
        if (include_addresses) obj.pushKV("addresses", std::move(a));
    }

    UniValue operator()(const CNoDestination& dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID& keyID) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("iswitness", false));
        if (pwalletMain && pwalletMain->GetPubKey(keyID, vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    UniValue operator()(const CScriptID& scriptID) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        obj.push_back(Pair("iswitness", false));
        if (pwalletMain && pwalletMain->GetCScript(scriptID, subscript)) {
            ProcessSubScript(subscript, obj, true);
        }
        return obj;
    }

    UniValue operator()(const WitnessV0KeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", 0));
        obj.push_back(Pair("witness_program", HexStr(id.begin(), id.end())));
        if (pwalletMain && pwalletMain->GetPubKey(CKeyID(id), vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
        }
        return obj;
    }

    UniValue operator()(const WitnessV0ScriptHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", 0));
        obj.push_back(Pair("witness_program", HexStr(id.begin(), id.end())));
        CRIPEMD160 hasher;
        uint160 hash;
        hasher.Write(id.begin(), 32).Finalize(hash.begin());
        if (pwalletMain && pwalletMain->GetCScript(CScriptID(hash), subscript)) {
            ProcessSubScript(subscript, obj);
        }
        return obj;
    }

    UniValue operator()(const WitnessUnknown& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", (int)id.version));
        obj.push_back(Pair("witness_program", HexStr(id.program, id.program + id.length)));
        return obj;
    }
};
#endif

/*
    Used for updating/reading spork settings on the network
*/
UniValue spork(const UniValue& params, bool fHelp)
{
    if (params.size() == 1 && params[0].get_str() == "show") {
        UniValue ret(UniValue::VOBJ);
        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();
        while (it != mapSporksActive.end()) {
            ret.push_back(Pair(sporkManager.GetSporkNameByID(it->second.nSporkID), it->second.nValue));
            it++;
        }
        return ret;
    } else if (params.size() == 2) {
        int nSporkID = sporkManager.GetSporkIDByName(params[0].get_str());
        if (nSporkID == -1) {
            return "Invalid spork name";
        }

        // SPORK VALUE
        int64_t nValue = params[1].get_int();

        //broadcast new spork
        if (sporkManager.UpdateSpork(nSporkID, nValue)) {
//            ExecuteSpork(nSporkID, nValue);
            return "success";
        } else {
            return "failure";
        }
    }

    throw runtime_error(
        "spork <name> [<value>]\n"
        "<name> is the corresponding spork name, or 'show' to show all current spork settings, active to show which sporks are active"
        "<value> is a epoch datetime to enable or disable spork" +
        HelpRequiringPassphrase());
}

UniValue validateaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress \"luxaddress\"\n"
            "\nReturn information about the given lux address.\n"
            "\nArguments:\n"
            "1. \"luxaddress\"     (string, required) The lux address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,       (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"luxaddress\",     (string) The lux address validated\n"
            "  \"hash160\" : \"hex\",            (string) The hex hash of the public key used in smart contracts\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"ismine\" : true|false,        (boolean) If the address is yours or not\n"
            "  \"iswatchonly\" : true|false,   (boolean) If the address is watchonly\n"
            "  \"isscript\" : true|false,      (boolean, optional) If the address is P2SH or P2WSH. Not included for unknown witness types.\n"
            "  \"iswitness\" : true|false,     (boolean) If the address is P2WPKH, P2WSH, or an unknown witness version\n"
            "  \"witness_version\" : version   (number, optional) For all witness output types, gives the version number.\n"
            "  \"witness_program\" : \"hex\"     (string, optional) For all witness output types, gives the script or key hash present in the address.\n"
            "  \"script\" : \"type\"             (string, optional) The output script type. Only if \"isscript\" is true and the redeemscript is known. Possible types: nonstandard, pubkey, pubkeyhash, scripthash, multisig, nulldata, witness_v0_keyhash, witness_v0_scripthash, witness_unknown\n"
            "  \"hex\" : \"hex\",                (string, optional) The redeemscript for the P2SH or P2WSH address\n"
            "  \"addresses\"                   (string, optional) Array of addresses associated with the known redeemscript (only if \"iswitness\" is false). This field is superseded by the \"pubkeys\" field and the address inside \"embedded\".\n"
            "    [\n"
            "      \"address\"\n"
            "      ,...\n"
            "    ]\n"
            "  \"pubkeys\"                     (string, optional) Array of pubkeys associated with the known redeemscript (only if \"script\" is \"multisig\")\n"
            "    [\n"
            "      \"pubkey\"\n"
            "      ,...\n"
            "    ]\n"
            "  \"sigsrequired\" : xxxxx        (numeric, optional) Number of signatures required to spend multisig output (only if \"script\" is \"multisig\")\n"
            "  \"pubkey\" : \"publickeyhex\",    (string, optional) The hex value of the raw public key, for single-key addresses (possibly embedded in P2SH or P2WSH)\n"
            "  \"embedded\" : {...},           (object, optional) information about the address embedded in P2SH or P2WSH, if relevant and known. It includes all validateaddress output fields for the embedded address, excluding \"isvalid\", metadata (\"timestamp\", \"hdkeypath\", \"hdmasterkeyid\") and relation to the wallet (\"ismine\", \"account\").\n"
            "  \"iscompressed\" : true|false,    (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"         (string) The account associated with the address, \"\" is the default account\n"
            "  \"timestamp\" : timestamp,      (number, optional) The creation time of the key if available in seconds since epoch (Jan 1 1970 GMT)\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"") + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\""));

    LOCK(cs_main);

    CTxDestination dest = DecodeDestination(params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid) {
        string currentAddress = EncodeDestination(dest);
        ret.push_back(Pair("address", currentAddress));
        uint160 hash160 = GetHashForDestination(dest);
        ret.push_back(Pair("hash160", hash160.ToStringReverseEndian()));
        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

#ifdef ENABLE_WALLET
        isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
        ret.push_back(Pair("ismine", bool(mine & ISMINE_SPENDABLE)));
        ret.push_back(Pair("iswatchonly", bool(mine & ISMINE_WATCH_ONLY)));
        UniValue detail = boost::apply_visitor(DescribeAddressVisitor(mine), dest);
        ret.pushKVs(detail);
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));
        if (pwalletMain) {
            const CKeyMetadata* meta = nullptr;
            CKeyID key_id = GetKeyForDestination(*pwalletMain, dest);
            if (key_id != uint160(0)) {
                auto it = pwalletMain->mapKeyMetadata.find(key_id);
                if (it != pwalletMain->mapKeyMetadata.end()) {
                    meta = &it->second;
                }
            }
            if (meta) {
                ret.push_back(Pair("timestamp", meta->nCreateTime));
            }
        }
#endif
    }
    return ret;
}

/**
 * Used by addmultisigaddress / createmultisig:
 */
/*CScript _createmultisig_redeemScript(const UniValue& params)
{
    int nRequired = params[0].get_int();
    const UniValue& keys = params[1].get_array();

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)",
                keys.size(), nRequired));
    if (keys.size() > 16)
        throw runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");
    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++) {
        const std::string& ks = keys[i].get_str();
#ifdef ENABLE_WALLET
        // Case 1: LUX address and we have full public key:
        CBitcoinAddress address(ks);
        if (pwalletMain && address.IsValid()) {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
                throw runtime_error(
                    strprintf("%s does not refer to a key", ks));
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(
                    strprintf("no full public key for address %s", ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: " + ks);
            pubkeys[i] = vchPubKey;
        }

        // Case 2: hex public key
        else
#endif
            if (IsHex(ks)) {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: " + ks);
            pubkeys[i] = vchPubKey;
        } else {
            throw runtime_error(" Invalid public key: " + ks);
        }
    }
    CScript result = GetScriptForMultisig(nRequired, pubkeys);

    if (result.size() > MAX_SCRIPT_ELEMENT_SIZE)
        throw runtime_error(
            strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE));

    return result;
}*/

UniValue createmultisig(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 2) {
        string msg = "createmultisig nrequired [\"key\",...]\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"

            "\nArguments:\n"
            "1. nrequired      (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"       (string, required) A json array of keys which are lux addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"key\"    (string) lux address or hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"       (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 addresses\n" +
            HelpExampleCli("createmultisig", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs a json rpc call\n" + HelpExampleRpc("createmultisig", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"");
        throw runtime_error(msg);
    }

    int required = params[0].get_int();

    // Get the public keys
    const UniValue& keys = params[1].get_array();
    std::vector<CPubKey> pubkeys;
    for (unsigned int i = 0; i < keys.size(); ++i) {
        if (IsHex(keys[i].get_str()) && (keys[i].get_str().length() == 66 || keys[i].get_str().length() == 130)) {
            pubkeys.push_back(HexToPubKey(keys[i].get_str()));
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Invalid public key: %s\nNote that from v0.16, createmultisig no longer accepts addresses."
            " Clients must transition to using addmultisigaddress to create multisig addresses with addresses known to the wallet before upgrading to v0.17."
            " To use the deprecated functionality, start bitcoind with -deprecatedrpc=createmultisig", keys[i].get_str()));
        }
    }

    // Construct using pay-to-script-hash:
    CScript inner = CreateMultisigRedeemscript(required, pubkeys);
    CScriptID innerID(inner);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("address", EncodeDestination(innerID)));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

    return result;
}

UniValue createwitnessaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
    {
        string msg = "createwitnessaddress \"script\"\n"
            "\nCreates a witness address for a particular script.\n"
            "It returns a json object with the address and witness script.\n"

            "\nArguments:\n"
            "1. \"script\"       (string, required) A hex encoded script\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new address (P2SH of witness script).\n"
            "  \"witnessScript\":\"script\"      (string) The string value of the hex-encoded witness script.\n"
            "}\n"
        ;
        throw runtime_error(msg);
    }

    if (!IsHex(params[0].get_str())) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Script must be hex-encoded");
    }

    std::vector<unsigned char> code = ParseHex(params[0].get_str());
    CScript script(code.begin(), code.end());
    CScript witscript = GetScriptForWitness(script);
    CScriptID witscriptid(witscript);
    CTxDestination dest(witscript);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("address", EncodeDestination(dest)));
    result.push_back(Pair("witnessScript", HexStr(witscript.begin(), witscript.end())));

    return result;
}

UniValue verifymessage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "verifymessage \"luxaddress\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"luxaddress\"  (string, required) The lux address to use for the signature.\n"
            "2. \"signature\"       (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"         (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n" +
            HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n" + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"my message\"") +
            "\nVerify the signature\n" + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"signature\" \"my message\"") +
            "\nAs json rpc\n" + HelpExampleRpc("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"signature\", \"my message\""));

    LOCK(cs_main);
    string strAddress   = params[0].get_str();
    string strSign      = params[1].get_str();
    string strMessage   = params[2].get_str();

    CTxDestination destination = DecodeDestination(strAddress);
    if (!IsValidDestination(destination))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    const CKeyID *keyID = boost::get<CKeyID>(&destination);
    if (!keyID)
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

UniValue setmocktime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setmocktime timestamp\n"
            "\nSet the local time to given timestamp (-regtest only)\n"
            "\nArguments:\n"
            "1. timestamp  (integer, required) Unix seconds-since-epoch timestamp\n"
            "   Pass 0 to go back to using the system time.");

    if (!Params().MineBlocksOnDemand())
        throw runtime_error("setmocktime for regression testing (-regtest mode) only");

    LOCK(cs_main);

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));
    SetMockTime(params[0].get_int64());

    return NullUniValue;
}

static bool getAddressFromIndex(const uint16_t type, const uint160 &hash, std::string &address)
{
    switch (type) {
        case 1: // keyhash
            address = EncodeDestination(CKeyID(uint160(hash)));
            break;
        case 2: // segwit
            address = EncodeDestination(CScriptID(uint160(hash)));
            break;
        case 4: // bech32
            address = EncodeDestination(WitnessV0KeyHash(uint160(hash)));
            break;
        default:
            return false;
    }
    return true;
}

static bool getAddressesFromParams(const UniValue& params, AddressTypeVector &addresses)
{
    if (params[0].isStr()) {

        CTxDestination dest = DecodeDestination(params[0].get_str());
        uint160 hashBytes = GetHashForDestination(dest);
        uint16_t hashType = dest.which();
        if (hashType == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        addresses.push_back(std::make_pair(hashBytes, hashType));

    } else if (params[0].isObject()) {

        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Addresses is expected to be an array");
        }

        std::vector<UniValue> values = addressValues.getValues();
        for (std::vector<UniValue>::iterator it = values.begin(); it != values.end(); ++it) {
            CTxDestination dest = DecodeDestination(it->get_str());
            uint160 hashBytes = GetHashForDestination(dest);
            uint16_t hashType = dest.which();
            if (hashType == 0)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            addresses.push_back(std::make_pair(hashBytes, hashType));
        }
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    return true;
}


static bool timestampSort(std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> a,
                          std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> b) {
    return a.second.time < b.second.time;
}

UniValue getaddressmempool(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressmempool\n"
            "\nReturns all mempool deltas for an address (-addressindex required).\n"
            "\nArguments: {\n"
            "  \"addresses: [\"\n"
            "    \"address\"     (string) The base58 address\n"
            "    ,...\n"
            "  ]\n"
            "}\n"
            "\nResult: [\n"
            "  {\n"
            "    \"address\",   (string) The base58check encoded address\n"
            "    \"txid\",      (string) The related transaction\n"
            "    \"index\",     (number) The related input or output index\n"
            "    \"satoshis\",  (number) The delta amount\n"
            "    \"timestamp\", (number) The time the transaction entered the mempool (seconds)\n"
            "    \"prevtx\",    (string) The previous tx hash (if spending)\n"
            "    \"prevout\"    (string) The previous transaction output index (if spending)\n"
            "  },...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressmempool", "\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"")
            + HelpExampleCli("getaddressmempool", "'{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}'")
            + HelpExampleRpc("getaddressmempool", "{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}")
        );

    AddressTypeVector addresses;
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    MempoolAddrDeltaVector indexes;
    if (!mempool.getAddressIndex(addresses, indexes)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
    }

    std::sort(indexes.begin(), indexes.end(), timestampSort);

    UniValue result(UniValue::VARR);

    for (MempoolAddrDeltaVector::iterator it = indexes.begin(); it != indexes.end(); it++) {

        std::string address;
        if (!getAddressFromIndex(it->first.hashType, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.push_back(Pair("address", address));
        delta.push_back(Pair("txid", it->first.txHash.GetHex()));
        delta.push_back(Pair("index", (int)it->first.indexInOut));
        delta.push_back(Pair("satoshis", it->second.amount));
        delta.push_back(Pair("timestamp", it->second.time));
        if (it->second.amount < 0) {
            delta.push_back(Pair("prevtx", it->second.prevhash.GetHex()));
            delta.push_back(Pair("prevout", (int)it->second.prevout));
        }
        result.push_back(delta);
    }

    return result;
}


static bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                       std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}

UniValue getaddressutxos(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressutxos\n"
            "\nReturns all unspent outputs for an address (-addressindex required).\n"
            "\nArguments: {\n"
            "  \"addresses: [\"\n"
            "    \"address\"  (string) The base58 address\n"
            "    ,...\n"
            "  ]\n"
            "}\n"
            "\nResult: [\n"
            "  {\n"
            "    \"address\",  (string) The base58 address\n"
            "    \"txid\",     (string) The transaction id\n"
            "    \"index\",    (number) The tx output index\n"
          //"    \"script\",   (string) The script hex encoded\n"
            "    \"satoshis\", (number) The amount of the output\n"
            "    \"height\"    (number) The block height\n"
            "  },...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressutxos", "\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"")
            + HelpExampleCli("getaddressutxos", "'{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}'")
            + HelpExampleRpc("getaddressutxos", "{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}")
        );

    AddressTypeVector addresses;
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    AddressUnspentVector unspentOutputs;
    for (AddressTypeVector::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressUnspent(it->first, it->second, unspentOutputs)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue result(UniValue::VARR);

    for (AddressUnspentVector::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        std::string address;
        if (!getAddressFromIndex(it->first.hashType, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.push_back(Pair("address", address));
        output.push_back(Pair("txid", it->first.txHash.GetHex()));
        output.push_back(Pair("index", (int)it->first.outputIndex));
      //output.push_back(Pair("script", HexStr(it->second.script.begin(), it->second.script.end())));
        output.push_back(Pair("satoshis", it->second.satoshis));
        output.push_back(Pair("height", it->second.blockHeight));
        result.push_back(output);
    }

    return result;
}

UniValue getaddressdeltas(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "getaddressdeltas\n"
            "\nReturns all changes for an address (-addressindex required).\n"
            "\nArguments: {\n"
            "  \"addresses: [\"\n"
            "    \"address\" (string) The base58 encoded address\n"
            "    ,...\n"
            "  ],\n"
            "  \"start\",    (number) The start block height (optional)\n"
            "  \"end\"       (number) The end block height (optional)\n"
            "}\n"
            "\nResult: [\n"
            "  {\n"
            "    \"satoshis\",   (number) The operation amount\n"
            "    \"height\",     (number) The block height\n"
            "    \"blockindex\", (number) The tx block index\n"
            "    \"txid\",       (string) The related transaction id\n"
            "    \"index\",      (number) The related input or output index\n"
            "    \"address\",    (string) The base58 encoded address\n"
            "    \"flags\"       (number) The type of movement\n"
            "  },...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressdeltas", "\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\" 0 10000")
            + HelpExampleCli("getaddressdeltas", "'{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"], \"start\": 0, \"end\": 35000}'")
            + HelpExampleRpc("getaddressdeltas", "{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"], \"start\": 0, \"end\": 35000}")
        );

    int start = 0, end = 0;
    if (params[0].isObject()) {
        UniValue startValue = find_value(params[0].get_obj(), "start");
        UniValue endValue = find_value(params[0].get_obj(), "end");
        if (startValue.isNum() && endValue.isNum()) {
            start = startValue.get_int();
            end = endValue.get_int();
        }
    } else if (params.size() == 3) {
        // debug console compat.
        if (params[1].isNum() && params[2].isNum()) {
            start = params[1].get_int();
            end = params[2].get_int();
        }
    }

    if (end == 0)
        end = chainActive.Height();
    if (end < start)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "End value is expected to be greater than start");

    AddressTypeVector addresses;
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    AddressIndexVector addressIndex;
    for (AddressTypeVector::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex(it->first, it->second, addressIndex, start, end)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    UniValue result(UniValue::VARR);

    for (AddressIndexVector::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        std::string address;
        if (!getAddressFromIndex(it->first.hashType, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.push_back(Pair("satoshis", it->second));
        delta.push_back(Pair("height", it->first.blockHeight));
        delta.push_back(Pair("blockindex", (int32_t)(it->first.blockIndex)));
        delta.push_back(Pair("txid", it->first.txhash.GetHex()));
        delta.push_back(Pair("index", (int32_t)(it->first.indexInOut)));
        delta.push_back(Pair("address", address));
        delta.push_back(Pair("flags", (int32_t)(it->first.spentFlags)));
        result.push_back(delta);
    }

    return result;
}

UniValue getaddressbalance(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressbalance\n"
            "\nReturns the balance for an address (-addressindex required).\n"
            "\nArguments: {\n"
            "  \"addresses\": [\n"
            "     \"address\" (string) The base58 address to query\n"
            "     ,...\n"
            "  ]\n"
            "}\n"
            "\nResult: {\n"
            "  \"balance\",  (number) The current balance (in LUX satoshis)\n"
            "  \"received\", (number) The total amount received (all outputs, including stake)\n"
            "  \"spent\",    (number) The total amount spent (excluding stakes)\n"
            "  \"sent\",     (number) The total amount sent (excl. stakes and amounts sent to same addr.)\n"
            "  \"staked\",   (number) The total amount of Proof of Stake incomes\n"
            "  \"deltas\"    (number) The total amount of movements indexed\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressbalance", "\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"")
            + HelpExampleCli("getaddressbalance", "'{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}'")
            + HelpExampleRpc("getaddressbalance", "{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}")
        );

    AddressTypeVector addresses;
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    AddressIndexVector addressIndex;

    for (AddressTypeVector::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    CAmount balance = 0;
    CAmount received = 0;
    CAmount spent = 0, sent = 0;
    CAmount staked = 0;
    uint256 lastStakeHash = uint256(-1);
    uint256 lastSpentHash = uint256(-1);

    for (AddressIndexVector::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        balance += it->second;

        if (it->second > 0) {
            // outputs
            received += it->second;
            if (it->first.txhash == lastStakeHash) {
                staked += it->second;
            } else if (it->first.txhash == lastSpentHash) {
                // sent to same address
                sent += it->second;
            }
        } else {
            // inputs
            if (it->first.spentFlags & ANDX_IS_STAKE) {
                staked += it->second;
                lastStakeHash = it->first.txhash;
            } else {
                sent += it->second;
                spent += it->second;
                lastSpentHash = it->first.txhash;
            }
        }
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("balance", balance));
    result.push_back(Pair("received", received));
    result.push_back(Pair("sent", (0 - sent)));
    result.push_back(Pair("spent", (0 - spent)));
    result.push_back(Pair("staked", staked));
    result.push_back(Pair("deltas", (int64_t)addressIndex.size()));

    return result;
}

UniValue getaddresstxids(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "getaddresstxids\n"
            "\nReturns the txids for an address(es) (-addressindex required).\n"
            "\nArguments: {\n"
            " \"addresses\": [\n"
            "   \"address\" (string) The base58 encoded address\n"
            "   ,...\n"
            "  ],\n"
            "  \"start\", (number) The start block height (optional)\n"
            "  \"end\"    (number) The end block height (optional)\n"
            "}\n"
            "\nResult: [\n"
            "  \"txid\"   (string) The transaction hash\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddresstxids", "\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\" 0 100000")
            + HelpExampleCli("getaddresstxids", "'{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}'")
            + HelpExampleRpc("getaddresstxids", "{\"addresses\": [\"LYmrT81UoxqfskSNt28ZKZ3XXskSFENEtg\"]}")
        );

    AddressTypeVector addresses;
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    int start = 0;
    int end = 0;
    if (params[0].isObject()) {
        UniValue startValue = find_value(params[0].get_obj(), "start");
        UniValue endValue = find_value(params[0].get_obj(), "end");
        if (startValue.isNum() && endValue.isNum()) {
            start = startValue.get_int();
            end = endValue.get_int();
        }
    } else if (params.size() == 3) {
        // debug console compat.
        if (params[1].isNum() && params[2].isNum()) {
            start = params[1].get_int();
            end = params[2].get_int();
        }
    }

    if (end == 0)
        end = chainActive.Height();
    if (end < start)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "End value is expected to be greater than start");

    AddressIndexVector addressIndex;
    for (AddressTypeVector::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex(it->first, it->second, addressIndex, start, end)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    std::set<std::pair<int, std::string> > txhashes;
    UniValue result(UniValue::VARR);

    for (AddressIndexVector::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        int height = it->first.blockHeight;
        std::string txhash = it->first.txhash.GetHex();

        if (addresses.size() > 1) {
            txhashes.insert(std::make_pair(height, txhash));
        } else {
            if (txhashes.insert(std::make_pair(height, txhash)).second) {
                result.push_back(txhash);
            }
        }
    }

    if (addresses.size() > 1) {
        for (std::set<std::pair<int, std::string> >::const_iterator it=txhashes.begin(); it!=txhashes.end(); it++) {
            result.push_back(it->second);
        }
    }

    return result;
}

UniValue getspentinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1) {
        throw runtime_error(
            "getspentinfo\n"
            "\nReturns the transaction where an output is spent. (-spentindex required)\n"
            "\nArguments: {\n"
            "  \"txid\": \"...\",   (string) The hash of the transaction\n"
            "  \"index\": 1       (number) The transaction output index to check\n"
            "}\n"
            "\nResult: {\n"
            "  \"txid\",   (string) The spent transaction hash\n"
            "  \"index\",  (number) The transaction input index\n"
            "  \"height\"  (number) The block height\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getspentinfo", "\"9e941e62e07c41e78dad34deab979fb49b4ef9f9c11113f0758b504f313d40a2\" 2")
            + HelpExampleCli("getspentinfo",
                "'{\"txhash\": \"9e941e62e07c41e78dad34deab979fb49b4ef9f9c11113f0758b504f313d40a2\", \"index\": 2}'")
            + HelpExampleRpc("getspentinfo",
                "{\"txhash\": \"9e941e62e07c41e78dad34deab979fb49b4ef9f9c11113f0758b504f313d40a2\", \"index\": 2}")
        );
    }

    uint256 txhash;
    int outputIndex = 0;
    if (params[0].isObject()) {
        // json rpc
        UniValue txhValue = find_value(params[0].get_obj(), "txid");
        UniValue indexValue = find_value(params[0].get_obj(), "index");
        if (!txhValue.isStr())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid tx");
        txhash = ParseHashV(txhValue, "txid");
        outputIndex = indexValue.isNum() ? indexValue.get_int() : 0;
    } else {
        // debug console compatible
        UniValue txhValue = params[0];
        if (!txhValue.isStr())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid tx");
        txhash = ParseHashV(txhValue, "txid");
        if (params.size() > 1) {
            UniValue indexValue = params[1];
            outputIndex = indexValue.isNum() ? indexValue.get_int() : 0;
        }
    }

    CSpentIndexKey key(txhash, outputIndex);
    CSpentIndexValue value;

    if (!GetSpentIndex(key, value)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get spent info");
    }

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txid", value.txhash.GetHex()));
    obj.push_back(Pair("index", (int)value.inputIndex));
    obj.push_back(Pair("height", value.blockHeight));

    return obj;
}

UniValue purgetxindex(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1) {
        throw runtime_error(
            "purgetxindex\n"
            "\nPurges an orphaned tx from the disk indexes.\n"
            "\nArgument:\n"
            "  \"txid\": \"...\" (string) The id of the transaction\n"
            "\nResult: {\n"
            "  \"txindex\": n, (number) The number of records purged from txindex\n"
            "  \"address\": n, (number) The number of records purged from addressindex\n"
            "  \"unspent\": n  (number) The number of records purged from unspentindex\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("purgetxindex", "\"6108f40537ae12df35d5c59b23de27f713efb11ff13b97607cde99d3a12257ce\"")
        );
    }
    uint256 txid;
    UniValue hex;
    if (params[0].isObject()) {
        hex = find_value(params[0].get_obj(), "txid");
    } else {
        hex = params[0];
    }
    if (!hex.isStr())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid txid");
    txid = ParseHashV(hex, "txid");

    CTransaction tx;
    uint256 hashBlock = uint256();
    if (GetTransaction(txid, tx, Params().GetConsensus(), hashBlock, true)) {
        if (hashBlock != uint256()) {
            CBlockIndex* pindex = LookupBlockIndex(hashBlock);
            if (pindex && chainActive.Contains(pindex))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "This tx is not orphaned!");
        }
    }

    int txs=0, addr=0, unspent=0;
    if (fTxIndex) {
        if (pblocktree->EraseTxIndex(txid)) txs++;
    }
    if (fAddressIndex) {
        AddressIndexVector addressIndex;
        if (pblocktree->FindTxEntriesInAddressIndex(txid, addressIndex)) {
            addr = (int) addressIndex.size();
            LogPrintf("addressindex: %zu records found for tx %s, deleting...\n", addressIndex.size(), txid.GetHex());
            pblocktree->EraseAddressIndex(addressIndex);
        }
        AddressUnspentVector addressUnspent;
        if (pblocktree->FindTxEntriesInUnspentIndex(txid, addressUnspent)) {
            unspent += (int) addressUnspent.size();
            LogPrintf("addressindex: %zu unspent keys found for tx %s, deleting...\n", addressUnspent.size(), txid.GetHex());
            pblocktree->EraseAddressUnspentIndex(addressUnspent);
        }
    }

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txindex", txs));
    obj.push_back(Pair("address", addr));
    obj.push_back(Pair("unspent", unspent));

    return obj;
}

#ifdef ENABLE_WALLET
UniValue getstakingstatus(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getstakingstatus\n"
            "Returns an object containing various staking information.\n"
            "\nResult:\n"
            "{\n"
            "  \"validtime\": true|false,          (boolean) if the chain tip is within staking phases\n"
            "  \"haveconnections\": true|false,    (boolean) if network connections are present\n"
            "  \"walletunlocked\": true|false,     (boolean) if the wallet is unlocked\n"
            "  \"mintablecoins\": true|false,      (boolean) if the wallet has mintable coins\n"
            "  \"enoughcoins\": true|false,        (boolean) if available coins are greater than reserve balance\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getstakingstatus", "") + HelpExampleRpc("getstakingstatus", ""));

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("validtime", chainActive.Tip()->nTime > 1471482000));
    obj.push_back(Pair("haveconnections", !vNodes.empty()));
    if (pwalletMain) {
        obj.push_back(Pair("walletunlocked", !pwalletMain->IsLocked()));
        obj.push_back(Pair("mintablecoins", pwalletMain->MintableCoins()));
        obj.push_back(Pair("enoughcoins", (stake->GetReservedBalance() <= pwalletMain->GetBalance() ? "yes" : "no")));
    }
    return obj;
}
#endif // ENABLE_WALLET

UniValue dfsannounce(const UniValue& params, bool fHelp)
{
    if (fHelp || !(params.size() == 3 || params.size() == 4))
        throw runtime_error(
                "dfsannounce \"localFilePath\", daysToKeep, fileCostInCoins\n"
                "\nAnnounces a file order.\n"
                "\nArgument:\n"
                "1. \"localFilePath\"         (string, required) The local file path of file\n"
                "2. daysToKeep                (numeric, required) The days to keep\n"
                "3. fileCostInCoins           (numeric, required) The file cost in coins\n"
                "4. maxGap                    (numeric, optional) ..TODO..\n"
                "\nResult:\n"
                "order hash\n"
                "\nExamples:\n"
                + HelpExampleCli("dfsannounce", "\"/tmp/123.jpg\" 1 0.3")
                + HelpExampleRpc("dfsannounce", "\"/tmp/123.jpg\", 1, 0.3")
        );

    if (storageController->address.IsTor() || !storageController->address.IsValid()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Unknown local IP address");
    }

    StorageOrder order{};
    boost::filesystem::path path{params[0].get_str()};

    order.fileURI = SerializeHash(path.string(), SER_GETHASH, 0);
    order.filename = path.filename().string();
    order.time = std::time(nullptr);
    order.storageUntil = order.time + std::stoi(params[1].get_str()) * 24 * 60 * 60;
    order.fileSize = boost::filesystem::file_size(path);
    order.maxRate = std::stoi(params[2].get_str());
    if (params.size() == 4) {
        order.maxGap = std::stoi(params[3].get_str());
    } else {
        order.maxGap = DEFAULT_STORAGE_MAX_BLOCK_GAP;
    }
    order.address = storageController->address;

    storageController->AnnounceOrder(order, path);
    storageController->proposalsAgent.ListenProposal(order.GetHash());

    UniValue ret(UniValue::VSTR);
    ret = order.GetHash().ToString();
    return ret;
}

UniValue dfscancelorder(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "dfscancelorder \"order's hash\"\n"
                        "\nCancel announces order.\n"
                        "\nArgument:\n"
                        "    \"hash order\"         (string, required) The hash of announces order\n"
                        "\nResult:\n"
                        "null\n"
                        "\nExamples:\n"
                + HelpExampleCli("dfscancelorder", "\"..TODO..\"")
                + HelpExampleRpc("dfscancelorder", "\"..TODO..\"")
        );

    uint256 hash = uint256{params[0].get_str()};
    storageController->CancelOrder(hash);

    return UniValue::VNULL;
}

UniValue dfsgetinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size())
        throw runtime_error(
                        "dfsgetinfo\n"
                        "Returns ..TODO..\n"
                        "\nResult:\n"
                        "{\n"
                        "  \"enabled\": true|false,     (boolean) ..TODO..\n"
                        "  \"myip\": \"...\",           (string) ..TODO..\n"
                        "  \"dfsfolder\": \"...\",      (string) path to data folder\n"
                        "  \"dfstempfolder\": \"...\",  (string) path to temp folder\n"
                        "  \"rate\": xxxxxxxx,          (numeric) ..TODO..\n"
                        "  \"maxblocksgap\": xxxxxx,    (numeric) max number of blocks, which can be mined between proofs\n"
                        "}\n"
                        "\nExamples:\n"
                + HelpExampleCli("dfsgetinfo", "") + HelpExampleRpc("dfsgetinfo", ""));

    std::string myip = storageController->address.ToStringIPPort();
    bool enabled = (myip != "[::]:0");

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("enabled", enabled));
    obj.push_back(Pair("myip", myip));
    obj.push_back(Pair("dfsfolder", storageController->storageHeap.GetChunks().back()->path));
    obj.push_back(Pair("dfstempfolder", storageController->tempStorageHeap.GetChunks().back()->path));
    obj.push_back(Pair("rate", storageController->rate));
    obj.push_back(Pair("maxblocksgap", storageController->maxblocksgap));

    return obj;
}

UniValue dfslistorders(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size())
        throw runtime_error(
                "dfslistorders\n"
                        "\nReturns array of orders\n"
                        "\nResult\n"
                        "[                   (array of json object)\n"
                        "  {\n"
                        "    \"orderhash\" : \"hash\",   (string) the order hash \n"
                        "    \"time\" : timestamp,       (numeric) the create order time\n"
                        "    \"filename\" : \"...\",     (string) ..TODO..\n"
                        "    \"address\" : \"address\",  (string) the ip address node which create order\n"
                        "    \"untiltime\" : timestamp,  (numeric) ..TODO..\n"
                        "    \"filesize\" : n,           (numeric) ..TODO..\n"
                        "    \"rate\" : n                (numeric) ..TODO..\n"
                        "    \"blockgap\" : n            (numeric) max number of blocks, which can be mined between proofs\n"
                        "  }\n"
                        "  ,...\n"
                        "]\n"
                        "\nExamples\n"
                + HelpExampleCli("dfslistorders", "")
                + HelpExampleRpc("dfslistorders", ""));

    UniValue result(UniValue::VARR);

    for (auto announcePair : storageController->mapAnnouncements){
        StorageOrder announce = announcePair.second;
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("orderhash", announcePair.first.ToString()));
        obj.push_back(Pair("time", announce.time));
        obj.push_back(Pair("fileURI", announce.fileURI.ToString()));
        obj.push_back(Pair("filename", announce.filename));
        obj.push_back(Pair("address", announce.address.ToStringIPPort()));
        obj.push_back(Pair("untiltime", announce.storageUntil));
        obj.push_back(Pair("filesize", announce.fileSize));
        obj.push_back(Pair("rate", announce.maxRate));
        obj.push_back(Pair("blockgap", announce.maxGap));
        result.push_back(obj);
    }

    return result;
}

UniValue dfslistproposals(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "dfslistproposals\n"
                        "\nReturns array of proposals for order\n"
                        "\nArguments:\n"
                        "    \"orders hash\"             (string, required) The hash of announces order\n"
                        "\nResult\n"
                        "[                   (array of json object)\n"
                        "  {\n"
                        "    \"order hash\" : \"hash\",  (string) the order hash \n"
                        "    \"time\" : timestamp,       (numeric) the create proposal time\n"
                        "    \"address\" : \"address\",  (string) the ip address node which create proposal\n"
                        "    \"rate\" : n                (numeric) ..TODO..\n"
                        "  }\n"
                        "  ,...\n"
                        "]\n"
                        "\nExamples\n"
                + HelpExampleCli("dfslistproposals", "..TODO..")
                + HelpExampleRpc("dfslistproposals", "..TODO.."));

    UniValue result(UniValue::VARR);
    std::vector<StorageProposal> proposals = storageController->proposalsAgent.GetProposals(uint256{params[0].get_str()});
    for (auto proposal : proposals){
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("orderhash", proposal.orderHash.ToString()));
        obj.push_back(Pair("time", proposal.time));
        obj.push_back(Pair("address", proposal.address.ToStringIPPort()));
        obj.push_back(Pair("rate", proposal.rate));
        result.push_back(obj);
    }

    return result;
}

UniValue dfslocalstorage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "dfslocalstorage\n"
                        "\nReturns array of local files visible from decentralized file storage\n"
                        "\nResult\n"
                        "[                   (array of json object)\n"
                        "  {\n"
                        "    ..TODO.. \n"
                        "  }\n"
                        "  ,...\n"
                        "]\n"
                        "\nExamples\n"
                + HelpExampleCli("dfslocalstorage", "..TODO..")
                + HelpExampleRpc("dfslocalstorage", "..TODO.."));

    UniValue result(UniValue::VARR);
    const auto chunks = storageController->storageHeap.GetChunks();

    size_t chunkIndex = 0;
    for(auto &&chunk : chunks) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("index", chunkIndex++));
        obj.push_back(Pair("type", "storage chunk"));
        obj.push_back(Pair("path", chunk->path));
        obj.push_back(Pair("totalSpace", std::to_string(chunk->totalSpace)));
        obj.push_back(Pair("freeSpace", std::to_string(chunk->freeSpace)));

        UniValue files(UniValue::VARR);
        for(auto &&file : chunk->files) {
            UniValue objFile(UniValue::VOBJ);
            objFile.push_back(Pair("filename", file->filename));
            objFile.push_back(Pair("uri", file->uri));
            objFile.push_back(Pair("size", std::to_string(file->size)));
            files.push_back(objFile);
        }
        obj.push_back(Pair("files", files));
        result.push_back(obj);
    }

    const auto tempChunks = storageController->tempStorageHeap.GetChunks();

    chunkIndex = 0;
    for(auto &&chunk : tempChunks) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("index", chunkIndex++));
        obj.push_back(Pair("type", "temp chunk"));
        obj.push_back(Pair("path", chunk->path));
        obj.push_back(Pair("totalSpace", std::to_string(chunk->totalSpace)));
        obj.push_back(Pair("freeSpace", std::to_string(chunk->freeSpace)));

        UniValue files(UniValue::VARR);
        for(auto &&file : chunk->files) {
            UniValue objFile(UniValue::VOBJ);
            objFile.push_back(Pair("filename", file->filename));
            objFile.push_back(Pair("uri", file->uri));
            objFile.push_back(Pair("size", std::to_string(file->size)));
            files.push_back(objFile);
        }
        obj.push_back(Pair("files", files));
        result.push_back(obj);
    }

    return result;
}

UniValue dfsacceptproposal(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "dfsacceptproposal\n"
                        "\n..TODO..\n"
                        "\nArguments:\n"
                        "    \"orders hash\"             (string, required) The hash of announces order\n"
                        "    \"proposals hash\"          (string, required) The hash of proposal\n"
                        "\nResult:\n"
                        "null\n"
                        "\nExamples\n"
                + HelpExampleCli("dfsacceptproposal", "..TODO..")
                + HelpExampleRpc("dfsacceptproposal", "..TODO.."));

    uint256 orderHash = uint256{params[0].get_str()};
    StorageProposal proposal = storageController->proposalsAgent.GetProposal(orderHash, uint256{params[1].get_str()});
    if (proposal.time != 0) {
        storageController->AcceptProposal(proposal);
    }

    return UniValue::VNULL;
}

UniValue dfssetparams(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "dfssetparams\n"
                        "\n..TODO..\n"
                        "\nArguments:\n"
                        "  \"min rate\": xxxxxxxx,       (numeric, required) ..TODO..\n"
                        "  \"max blocks gap\": xxxxxx,   (numeric, required) ..TODO..\n"
                        "\nResult:\n"
                        "null\n"
                        "\nExamples\n"
                + HelpExampleCli("dfssetparams", "1 20")
                + HelpExampleRpc("dfssetparams", "1 20"));

    CAmount rate = std::stoi(params[0].get_str());
    size_t maxblocksgap = std::stoi(params[1].get_str());

    if (rate > COIN && maxblocksgap > 10000) {
        LogPrintf("Warning! %s: dfs parameters are huge (rate: %d gap: %d)\n", __func__, rate, maxblocksgap);
    }

    storageController->rate = rate;
    storageController->maxblocksgap = maxblocksgap;

    return UniValue::VNULL;
}

UniValue dfssetfolder(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "dfssetfolder\n"
                        "\nSet path to dfs data folder\n"
                        "\nArguments:\n"
                        "  \"dfs folder\": \"...\",      (string) path to dfs data folder\n"
                        "\nResult:\n"
                        "null\n"
                        "\nExamples\n"
                + HelpExampleCli("dfssetfolder", "/home/user/dfs")
                + HelpExampleRpc("dfssetfolder", "/home/user/dfs"));

    namespace fs = boost::filesystem;
    fs::path path{params[0].get_str()};

    if (!fs::is_directory(path)) {
        UniValue obj(UniValue::VSTR, "Error! dfssetfolder: directory" + path.string() + " isn't exists");
        return obj;
    }

    auto chunksSize = storageController->storageHeap.GetChunks().size();
    storageController->storageHeap.MoveChunk(chunksSize - 1, path);

    UniValue obj(UniValue::VSTR, "dfssetfolder: move chunk to " + path.string() + " directory");
    return obj;
}

UniValue dfssettempfolder(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "dfssettempfolder\n"
                        "\nSet path to dfs temp folder\n"
                        "\nArguments:\n"
                        "  \"dfs folder\": \"...\",      (string) path to dfs temp folder\n"
                        "\nResult:\n"
                        "null\n"
                        "\nExamples\n"
                + HelpExampleCli("dfssettempfolder", "/home/user/temp")
                + HelpExampleRpc("dfssettempfolder", "/home/user/temp"));

    namespace fs = boost::filesystem;
    fs::path path{params[0].get_str()};

    if (!fs::is_directory(path)) {
        UniValue obj(UniValue::VSTR, "Error! dfssettempfolder: directory" + path.string() + " isn't exists");
        return obj;
    }

    auto chunksSize = storageController->tempStorageHeap.GetChunks().size();
    storageController->tempStorageHeap.MoveChunk(chunksSize - 1, path);

    UniValue obj(UniValue::VSTR, "dfssettempfolder: move temp chunk to " + path.string() + " directory");
    return obj;
}

UniValue dfsremoveoldorders(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "dfsremoveoldorders\n"
                        "\n..TODO..\n"
                        "\nArguments:\n"
                        "  \"time\": xxxxxxxx,       (numeric, optional) ..TODO..\n"
                        "\nResult:\n"
                        "null\n"
                        "\nExamples\n"
                + HelpExampleCli("dfsremoveoldorders", "1546300800")
                + HelpExampleRpc("dfsremoveoldorders", "1546300800"));

    std::time_t timestamp;
    if (params.size()) {
        timestamp = std::stoi(params[0].get_str());
    } else {
        timestamp = time_t(nullptr) - 7 * 24 * 60 * 60; // one week ago
    }

    storageController->ClearOldAnnouncments(timestamp);

    return UniValue::VNULL;
}

