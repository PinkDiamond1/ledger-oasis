/*******************************************************************************
*   (c) 2019 ZondaX GmbH
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/
#include "testcases.h"
#include "base64.h"
#include <fmt/core.h>
#include <crypto.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <utility>
#include <parser_impl.h>
#include "consumer/coin_consumer.h"
#include <zxformat.h>

namespace utils {
    std::vector<uint8_t> prepareBlob(const std::string &context, const std::string &base64Cbor) {
        std::string cborString;
        macaron::Base64::Decode(base64Cbor, cborString);

        if (context.size() >= 256) {
            throw std::invalid_argument("context should be < 256 bytes");
        }

        // Allocate and prepare buffer
        // context size
        // context
        // CBOR payload
        uint16_t bufferLen = 1 + context.size() + cborString.size();
        auto bufferAllocation = std::vector<uint8_t>(bufferLen);

        char tmp[1000];
        array_to_hexstr(tmp, sizeof(tmp), (uint8_t *) cborString.c_str(), cborString.size());
        std::cout << tmp << std::endl;

        bufferAllocation[0] = context.size();
        MEMCPY(bufferAllocation.data() + 1, context.c_str(), context.size());
        MEMCPY(bufferAllocation.data() + 1 + context.size(), cborString.c_str(), cborString.size());

        return bufferAllocation;
    }

    testcaseData_t ReadTestCaseData(const std::shared_ptr<Json::Value> &jsonSource, int index) {
        testcaseData_t answer;
        auto v = (*jsonSource)[index];
        auto description = std::string("");
        auto valid_tx = true;

        if (v.isMember("valid_tx")) {
            valid_tx = v["valid_tx"].asBool();
        }

        if (v.isMember("description")) {
            description = v["description"].asString();
        } else {
            description = v["kind"].asString();
        }

        description.erase(remove_if(description.begin(), description.end(), isspace), description.end());

        auto encoded_tx = v["encoded_tx"].asString();

        if (v.isMember("entity_meta")) {
            encoded_tx = v["encoded_entity_meta"].asString();
        }

        return {
                false,
                description,
                std::to_string(index),
                v["kind"].asString(),
                v["signature_context"].asString(),
                encoded_tx,
                v["valid"].asBool() && TestcaseIsValid(v),
                valid_tx,
                GenerateExpectedUIOutput(v["signature_context"].asString(), v)
        };
    }

    std::vector<testcase_t> GetJsonTestCases(const std::string &filename) {
        auto answer = std::vector<testcase_t>();

        Json::CharReaderBuilder builder;
        std::shared_ptr<Json::Value> obj(new Json::Value());

        std::string fullPathJsonFile = std::string(TESTVECTORS_DIR) + filename;
        std::ifstream inFile(fullPathJsonFile);
        if (!inFile.is_open())
            return answer;

        // Retrieve all test cases
        JSONCPP_STRING errs;
        Json::parseFromStream(builder, inFile, obj.get(), &errs);
        std::cout << "Number of testcases: " << obj->size() << std::endl;
        answer.reserve(obj->size());

        for (int i = 0; i < obj->size(); i++) {
            auto v = (*obj)[i];
            auto description = std::string("");

            if (v.isMember("description")) {
                description = v["description"].asString();
            } else {
                description = v["kind"].asString();
            }
            description.erase(remove_if(description.begin(), description.end(), isspace), description.end());

            answer.push_back(testcase_t{obj, i, description});
        }

        return answer;
    }

    std::string FormatSignature(const std::string &sig, uint8_t idx, uint8_t *pageCount) {
        std::string sigBytes;
        macaron::Base64::Decode(sig, sigBytes);

        char tmp[1000];
        MEMZERO(tmp, sizeof(tmp));
        array_to_hexstr(tmp, sizeof(tmp), (const uint8_t *) sigBytes.c_str(), sigBytes.size());

        char outBuffer[40];
        pageString(outBuffer, sizeof(outBuffer), tmp, idx, pageCount);

        return std::string(outBuffer);
    }

    std::string FormatPublicKey(const std::string &pk, uint8_t idx, uint8_t *pageCount) {
        std::string pkBytes;
        macaron::Base64::Decode(pk, pkBytes);

        char tmp[100];
        MEMZERO(tmp, sizeof(tmp));
        array_to_hexstr(tmp, sizeof(tmp), (const uint8_t *) pkBytes.c_str(), pkBytes.size());

        char outBuffer[40];
        pageString(outBuffer, sizeof(outBuffer), tmp, idx, pageCount);

        return std::string(outBuffer);
    }
    
    std::string FormatPublicKey_B64(const std::string &pk, uint8_t idx, uint8_t *pageCount) {
        std::string pkBytes;
        macaron::Base64::Decode(pk, pkBytes);

        char outBuffer[40];
        pageString(outBuffer, sizeof(outBuffer), pk.c_str(), idx, pageCount);

        return std::string(outBuffer);
    }

    std::string FormatAddress(const std::string &address, uint8_t idx, uint8_t *pageCount) {
        char outBuffer[40];
        pageString(outBuffer, sizeof(outBuffer), address.c_str(), idx, pageCount);
        return std::string(outBuffer);
    }

    std::string FormatHash(const std::string &hash, uint8_t idx, uint8_t *pageCount) {
        char outBuffer[40];
        pageString(outBuffer, sizeof(outBuffer), hash.c_str(), idx, pageCount);
        return std::string(outBuffer);
    }

    std::string FormatAmount(const std::string &amount) {
        char buffer[500];
        MEMZERO(buffer, sizeof(buffer));
        fpstr_to_str(buffer, sizeof(buffer), amount.c_str(), COIN_AMOUNT_DECIMAL_PLACES);
        number_inplace_trimming(buffer, 1);
        return std::string(buffer);
    }

    std::string FormatShares(const std::string &amount) {
        char buffer[500];
        MEMZERO(buffer, sizeof(buffer));
        fpstr_to_str(buffer, sizeof(buffer), amount.c_str(), 0);
        number_inplace_trimming(buffer, 1);
        return std::string(buffer);
    }

    std::string FormatRate(const std::string &rate) {
        char buffer[500];
        MEMZERO(buffer, sizeof(buffer));
        // This is shifting two decimal places to show as a percentage
        fpstr_to_str(buffer, sizeof(buffer), rate.c_str(), COIN_RATE_DECIMAL_PLACES - 2);
        return std::string(buffer) + "%";
    }

    std::string FormatRates(const Json::Value &rates, uint8_t idx, uint8_t *pageCount) {
        *pageCount = rates.size() * 2;
        if (idx < *pageCount) {
            auto r = rates[idx / 2];
            switch (idx % 2) {
                case 0:
                    return fmt::format("({}): start : {}", idx / 2 + 1, r["start"].asUInt64());
                case 1:
                    return fmt::format("({}): rate : {}", idx / 2 + 1, FormatRate(r["rate"].asString()));
            }
        }

        return "";
    }

    std::string FormatBounds(const Json::Value &bounds, uint8_t idx, uint8_t *pageCount) {
        *pageCount = bounds.size() * 3;
        if (idx < *pageCount) {
            auto r = bounds[idx / 3];
            switch (idx % 3) {
                case 0:
                    return fmt::format("({}): start : {}", idx / 3 + 1, r["start"].asUInt64());
                case 1:
                    return fmt::format("({}): min : {}", idx / 3 + 1, FormatRate(r["rate_min"].asString()));
                case 2:
                    return fmt::format("({}): max : {}", idx / 3 + 1, FormatRate(r["rate_max"].asString()));
            }
        }

        return "";
    }

    std::string FormatVote(uint8_t vote) {
        switch (vote) {
            case 1:
                return "yes";
            case 2:
                return "no";
            case 3:
                return "abstain";
        }
        return "";
    }

    std::string FormatVersion(const Json::Value &version) {
        auto major = 0;
        auto minor = 0;
        auto patch = 0;

        if( version.isMember("major") ){
            major = version["major"].asUInt64();
        }

        if( version.isMember("minor") ){
            minor = version["minor"].asUInt64();
        }

        if( version.isMember("patch") ){
            patch = version["patch"].asUInt64();
        }

        if (major != 0 || minor != 0 || patch != 0) {
            return fmt::format("{}.{}.{}", major, minor, patch);
        } else {
            return "-";
        }
    }

    bool TestcaseIsValid(const Json::Value &tc) {
        if (tc["kind"] == "AmendCommissionSchedule") {
            auto rates = tc["tx"]["body"]["amendment"]["rates"];
            auto bounds = tc["tx"]["body"]["amendment"]["bounds"];
            if (rates.empty()) {
                return false;
            }
            if (bounds.empty()) {
                return false;
            }
        }
        return true;
    }

    template<typename S, typename... Args>
    void addTo(std::vector<std::string> &answer, const S &format_str, Args &&... args) {
        answer.push_back(fmt::format(format_str, args...));
    }

    std::vector<std::string> internalGenerateExpectedUIOutputForEntity(Json::Value entity, uint32_t &itemCount) {
        auto answer = std::vector<std::string>();
        uint8_t dummy = 0;

        auto entity_id = entity["id"].asString();
        addTo(answer, "{} | ID[1/2] : {}", itemCount, FormatPublicKey(entity_id, 0, &dummy));
        addTo(answer, "{} | ID[2/2] : {}", itemCount++, FormatPublicKey(entity_id, 1, &dummy));

        int nodeIndex = 0;
        for (nodeIndex = 0; nodeIndex < entity["nodes"].size(); nodeIndex++) {
            auto nodeData = entity["nodes"][nodeIndex].asString();
            addTo(answer, "{} | Node [{}][1/2] : {}", itemCount, nodeIndex + 1, FormatPublicKey(nodeData, 0, &dummy));
            addTo(answer, "{} | Node [{}][2/2] : {}", itemCount, nodeIndex + 1, FormatPublicKey(nodeData, 1, &dummy));
            itemCount++;
        }

        return answer;
    }

    std::vector<std::string> internalGenerateExpectedUIOutputForEntity_B64(Json::Value entity, uint32_t &itemCount) {
        auto answer = std::vector<std::string>();
        uint8_t dummy = 0;

        auto entity_id = entity["id"].asString();
        addTo(answer, "{} | ID[1/2] : {}", itemCount, FormatPublicKey_B64(entity_id, 0, &dummy));
        addTo(answer, "{} | ID[2/2] : {}", itemCount++, FormatPublicKey_B64(entity_id, 1, &dummy));

        int nodeIndex = 0;
        for (nodeIndex = 0; nodeIndex < entity["nodes"].size(); nodeIndex++) {
            auto nodeData = entity["nodes"][nodeIndex].asString();
            addTo(answer, "{} | Node [{}][1/2] : {}", itemCount, nodeIndex + 1, FormatPublicKey_B64(nodeData, 0, &dummy));
            addTo(answer, "{} | Node [{}][2/2] : {}", itemCount, nodeIndex + 1, FormatPublicKey_B64(nodeData, 1, &dummy));
            itemCount++;
        }

        return answer;
    }

    std::vector<std::string> GenerateExpectedUIOutputForEntity(Json::Value j, uint32_t &itemCount) {
        auto answer = std::vector<std::string>();

        addTo(answer, "{} | Sign : Entity", itemCount++);
        auto answerEntity = internalGenerateExpectedUIOutputForEntity(std::move(j), itemCount);
        answer.insert(answer.end(), answerEntity.begin(), answerEntity.end());

        return answer;
    }

    std::vector<std::string> GenerateExpectedUIOutputForTx(Json::Value j, uint32_t &itemCount) {
        auto answer = std::vector<std::string>();

        auto type = j["tx"]["method"].asString();
        auto tx = j["tx"];
        auto txbody = tx["body"];

        uint8_t dummy = 0;

        if (type == "staking.Transfer") {
            addTo(answer, "{} | Type : Transfer", itemCount++);
            addTo(answer, "{} | To[1/2] : {}", itemCount, FormatAddress(txbody["to"].asString(), 0, &dummy));
            addTo(answer, "{} | To[2/2] : {}", itemCount++, FormatAddress(txbody["to"].asString(), 1, &dummy));
            addTo(answer, "{} | Amount : {} {}", itemCount++, COIN_DENOM, FormatAmount(txbody["amount"].asString()));
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "staking.Burn") {
            addTo(answer, "{} | Type : Burn", itemCount++);
            addTo(answer, "{} | Amount : {} {}", itemCount++, COIN_DENOM, FormatAmount(txbody["amount"].asString()));
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "staking.Allow") {
            addTo(answer, "{} | Type : Allow", itemCount++);

            auto allowAccount = txbody["beneficiary"].asString();
            addTo(answer, "{} | Beneficiary[1/2] : {}", itemCount, FormatAddress(allowAccount, 0, &dummy));
            addTo(answer, "{} | Beneficiary[2/2] : {}", itemCount++, FormatAddress(allowAccount, 1, &dummy));

            std::string sign;
            auto negative = txbody["negative"].asBool();
            if(negative){
                sign = "-";
            }else {
                sign = "+";
            }
            addTo(answer, "{} | Amount change : {} {}{}", itemCount++, COIN_DENOM, sign, FormatAmount(txbody["amount_change"].asString()));

            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "staking.Withdraw") {
            addTo(answer, "{} | Type : Withdraw", itemCount++);

            auto withdrawAccount = txbody["from"].asString();
            addTo(answer, "{} | From[1/2] : {}", itemCount, FormatAddress(withdrawAccount, 0, &dummy));
            addTo(answer, "{} | From[2/2] : {}", itemCount++, FormatAddress(withdrawAccount, 1, &dummy));

            addTo(answer, "{} | Amount : {} {}", itemCount++, COIN_DENOM, FormatAmount(txbody["amount"].asString()));
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "staking.AddEscrow") {
            addTo(answer, "{} | Type : Add escrow", itemCount++);

            auto escrowAccount = txbody["account"].asString();
            addTo(answer, "{} | To[1/2] : {}", itemCount, FormatAddress(escrowAccount, 0, &dummy));
            addTo(answer, "{} | To[2/2] : {}", itemCount++, FormatAddress(escrowAccount, 1, &dummy));

            addTo(answer, "{} | Amount : {} {}", itemCount++, COIN_DENOM, FormatAmount(txbody["amount"].asString()));

            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "staking.ReclaimEscrow") {
            addTo(answer, "{} | Type : Reclaim escrow", itemCount++);

            auto escrowAccount = txbody["account"].asString();
            addTo(answer, "{} | From[1/2] : {}", itemCount, FormatAddress(escrowAccount, 0, &dummy));
            addTo(answer, "{} | From[2/2] : {}", itemCount++, FormatAddress(escrowAccount, 1, &dummy));

            addTo(answer, "{} | Shares : {}", itemCount++, FormatShares(txbody["shares"].asString()));
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "staking.AmendCommissionSchedule") {
            addTo(answer, "{} | Type : Amend commission schedule", itemCount++);

            uint8_t pageIdx = 0;
            uint8_t pageCount = 1;
            while (pageIdx < pageCount) {
                auto s = FormatRates(txbody["amendment"]["rates"], pageIdx, &pageCount);
                if (!s.empty())
                    addTo(answer, "{} | Rates {}", itemCount, s);
                pageIdx++;
                itemCount++;
            }
            pageIdx = 0;
            pageCount = 1;
            while (pageIdx < pageCount) {
                auto s = FormatBounds(txbody["amendment"]["bounds"], pageIdx, &pageCount);
                if (!s.empty())
                    addTo(answer, "{} | Bounds {}", itemCount, s);
                pageIdx++;
                itemCount++;
            }

            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "registry.UnfreezeNode") {
            addTo(answer, "{} | Type : Unfreeze Node", itemCount++);
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
            auto publicKey = txbody["node_id"].asString();
            addTo(answer, "{} | Node ID[1/2] : {}", itemCount, FormatPublicKey(publicKey, 0, &dummy));
            addTo(answer, "{} | Node ID[2/2] : {}", itemCount++, FormatPublicKey(publicKey, 1, &dummy));
        }

        if (type == "registry.RegisterEntity") {
            addTo(answer, "{} | Type : Register Entity", itemCount++);

            // Entity (from entity)
            auto untrusted_raw_value = j["tx"]["body"]["untrusted_raw_value"];
            auto entityAnswer = internalGenerateExpectedUIOutputForEntity_B64(untrusted_raw_value, itemCount);
            answer.insert(answer.end(), entityAnswer.begin(), entityAnswer.end());

            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "registry.DeregisterEntity") {
            addTo(answer, "{} | Type : Deregister Entity", itemCount++);

            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "governance.CastVote") {
            addTo(answer, "{} | Type : Cast vote", itemCount++);
            addTo(answer, "{} | Proposal ID : {}", itemCount++, txbody["id"].asString());
            addTo(answer, "{} | Vote : {}", itemCount++,txbody["vote"].asString());
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        if (type == "governance.SubmitProposal") {
            addTo(answer, "{} | Type : Submit proposal", itemCount++);
            if(txbody.isMember("upgrade")){
                addTo(answer, "{} | Kind : Upgrade", itemCount++);
                addTo(answer, "{} | Handler : {}", itemCount++, txbody["upgrade"]["handler"].asString());
                addTo(answer, "{} | Consensus : {}", itemCount++, FormatVersion(txbody["upgrade"]["target"]["consensus_protocol"]));
                addTo(answer, "{} | Runtime Host : {}", itemCount++, FormatVersion(txbody["upgrade"]["target"]["runtime_host_protocol"]));
                addTo(answer, "{} | Runtime Committee : {}", itemCount++, FormatVersion(txbody["upgrade"]["target"]["runtime_committee_protocol"]));
                addTo(answer, "{} | Epoch : {}", itemCount++, txbody["upgrade"]["epoch"].asUInt64());
            }
            if(txbody.isMember("cancel_upgrade")){
                addTo(answer, "{} | Kind : Cancel upgrade", itemCount++);
                addTo(answer, "{} | Proposal ID : {}", itemCount++, txbody["cancel_upgrade"]["proposal_id"].asUInt64());
            }
            if (tx.isMember("fee")) {
                addTo(answer, "{} | Fee : {} {}", itemCount++, COIN_DENOM, FormatAmount(tx["fee"]["amount"].asString()));
                addTo(answer, "{} | Gas limit : {}", itemCount++, tx["fee"]["gas"].asUInt64());
            }
        }

        return answer;
    }

    std::vector<std::string> GenerateExpectedUIOutputForEntityMetadata(Json::Value j, uint32_t &itemCount) {
        auto answer = std::vector<std::string>();

        auto entity_meta = j["entity_meta"];

        addTo(answer, "{} | Sign : Entity metadata", itemCount++);
        addTo(answer, "{} | Version : {}", itemCount++, entity_meta["v"].asUInt64());
        addTo(answer, "{} | Serial : {}", itemCount++, entity_meta["serial"].asUInt64());

        if (entity_meta.isMember("name")) {
            addTo(answer, "{} | Name : {}", itemCount++, entity_meta["name"].asString());
        }
        if (entity_meta.isMember("url")) {
            addTo(answer, "{} | URL : {}", itemCount++, entity_meta["url"].asString());
        }
        if (entity_meta.isMember("email")) {
            addTo(answer, "{} | Email : {}", itemCount++, entity_meta["email"].asString());
        }
        if (entity_meta.isMember("keybase")) {
            addTo(answer, "{} | Keybase : {}", itemCount++, entity_meta["keybase"].asString());
        }
        if (entity_meta.isMember("twitter")) {
            addTo(answer, "{} | Twitter : {}", itemCount++, entity_meta["twitter"].asString());
        }

        return answer;
    }

    std::vector<std::string>  GenerateExpectedUIOutput(std::string context, const Json::Value &j) {
        auto answer = std::vector<std::string>();
        uint32_t itemCount = 0;

        if (!TestcaseIsValid(j)) {
            answer.emplace_back("Test case is not valid!");
            return answer;
        }

        // Entity or tx ?
        auto kind = j["kind"].asString();

        if (j.isMember("tx")) {
            // is tx
            answer = GenerateExpectedUIOutputForTx(j, itemCount);
        } else {
            // is entity
            if (j.isMember("entity_meta")) {
                answer = GenerateExpectedUIOutputForEntityMetadata(j, itemCount);
            } else {
                answer = GenerateExpectedUIOutputForEntity(j, itemCount);
            }
        }

        auto expectedPrefix1 = std::string(context_prefix_tx);
        auto expectedPrefix2 = std::string(context_prefix_entity);

        std::string contextSuffix;

        auto find1 = context.find(expectedPrefix1);
        if (find1 != std::string::npos) {
            uint8_t dummy;
            contextSuffix = context.replace(context.find(expectedPrefix1), expectedPrefix1.size(), "");
            addTo(answer, "{} | Network[1/2] : {}", itemCount, FormatHash(contextSuffix, 0, &dummy));
            addTo(answer, "{} | Network[2/2] : {}", itemCount, FormatHash(contextSuffix, 1, &dummy));
        }

        auto find2 = context.find(expectedPrefix2);
        if (find2 != std::string::npos) {
            contextSuffix = context.replace(context.find(expectedPrefix2), expectedPrefix2.size(), "");
        }

        return answer;
    }
}
