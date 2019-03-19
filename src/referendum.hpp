#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>

#include <string>
#include <vector>
#include <algorithm>

namespace referendum{
    using eosio::const_mem_fun;
    using eosio::datastream;
    using eosio::indexed_by;
    using eosio::name;
    using eosio::asset;
    using eosio::time_point_sec;
    using std::function;
    using std::string;
    using std::vector;

    class [[eosio::contract("referendum_contract")]] referendum_contract : public eosio::contract{

        public:
            using contract::contract;

            referendum_contract( name s, name code, datastream<const char*> ds )
                    :contract(s,code,ds),
                     _counter(_self, _self.value){}
            //explicit referendum_contract(uint64_t get_self());

            [[eosio::action]]
        void makeproposal(name creator, const string& proposal_name, const string& content);

        [[eosio::action]]
        void transfer(uint64_t sender, uint64_t receiver);

        [[eosio::action]]
        void vote(name voter, uint64_t id, bool is_agree);

        private:
            struct account {
                asset    balance;

                uint64_t primary_key() const { return balance.symbol.code().raw(); }
            };

            typedef eosio::multi_index<name("accounts"), account> accounts;

            // taken from eosio.token.hpp
            struct st_transfer {
                name  from;
                name  to;
                asset         quantity;
                std::string   memo;
            };

            struct [[eosio::table]] proposal{
                uint64_t id;
                name creator;
                string proposal_name;
                string content;
                int64_t yes;
                int64_t no;

                uint64_t primary_key() const { return creator.value; }
                uint64_t by_id() const { return static_cast<uint64_t>(id); }
            };

            typedef eosio::multi_index< name("proposals"), proposal,
                    indexed_by< name("idx"), const_mem_fun<proposal, uint64_t, &proposal::by_id>  >
            > proposals;

            //typedef eosio::multi_index<name("proposals"), proposal> proposals;

            struct [[eosio::table]] voter{
                name voter;
                int64_t amount;
                vector<int64_t> agreed_proposal_ids;
                vector<int64_t> disagreed_proposal_ids;

                uint64_t primary_key() const { return voter.value; }
            };

            typedef eosio::multi_index<name("voters"), voter> voters;

            struct counter{
                uint64_t global_id = 0;

                EOSLIB_SERIALIZE(counter, (global_id));
            };

            typedef eosio::singleton< name("counter"), counter > counter_singleton;

            counter_singleton _counter;

    };
} //referendum