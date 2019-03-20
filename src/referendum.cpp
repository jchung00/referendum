#include "referendum.hpp"

namespace referendum{

    void referendum_contract::makeproposal(name creator, const string& proposal_name, const string& content){
        require_auth(creator);
        eosio_assert(proposal_name.size() < 200, "proposal name is too long");
        eosio_assert(content.size() < 16384, "proposal content is too long");

        proposals _proposals(_self, _self.value);

        counter_singleton _counter(_self, _self.value);

        if(_counter.begin() == _counter.end()){
            _counter.emplace(get_self(), [&](auto& c){
               c.global_id = 0;
            });
        }

        auto count = _counter.find(0);

        _proposals.emplace(creator, [&](auto& proposal){
            proposal.id = (*count).global_id;
            proposal.creator = creator;
            proposal.proposal_name = proposal_name;
            proposal.content = content;
            proposal.yes = 0;
            proposal.no = 0;
        });

        _counter.modify(_counter.begin(), get_self(), [&](auto& c){
           c.global_id++;
        });
    }

    void referendum_contract::transfer(name sender, name receiver){
        auto transfer_data = eosio::unpack_action_data<st_transfer>();

        if(transfer_data.from == get_self()){
            return;
        }

        eosio_assert(transfer_data.from == get_self() || transfer_data.to == get_self(), "Invalid transfer");

        eosio_assert(transfer_data.quantity.is_valid(), "Invalid asset");

        voters _voters(_self, _self.value);

        auto itr = _voters.find(sender.value);

        if(itr == _voters.end()){
            _voters.emplace(_self, [&](auto& v){
                v.voter = sender;
                v.tokens = transfer_data.quantity;
                v.amount = transfer_data.quantity.amount;
            });
        }
        else{
            _voters.modify(itr, get_self(), [&](auto& v){
                v.tokens += transfer_data.quantity;
                v.amount += transfer_data.quantity.amount;
            });
        }
    }

    void referendum_contract::vote(name voter, uint64_t id, bool is_agree){
        require_auth(voter);
        voters _voters(_self, _self.value);

        proposals _proposals(_self, _self.value);

        auto idx_index = _proposals.get_index<name("idx")>();
        auto itr_proposal = idx_index.find(id);
        eosio_assert(itr_proposal != idx_index.end(), "Proposal not found");

        auto itr = _voters.find(voter.value);
        if(itr == _voters.end()){
            _voters.emplace(_self, [&](auto& v){
                v.voter = voter;
                v.amount = 0;
                if(is_agree){
                    v.agreed_proposal_ids.push_back(id);
                }
                else{
                    v.disagreed_proposal_ids.push_back(id);
                }
            });
        }
        else{
            if(is_agree){
                //delete existing vote on same proposal
                auto itr_voted_proposals_d = std::find((*itr).disagreed_proposal_ids.begin(), (*itr).disagreed_proposal_ids.end(), id);
                if(itr_voted_proposals_d != (*itr).disagreed_proposal_ids.end()){
                    _voters.modify(itr, get_self(), [&](auto& v){
                        v.disagreed_proposal_ids.erase(itr_voted_proposals_d);
                    });
                    idx_index.modify(itr_proposal, get_self(), [&](auto& proposal){
                        proposal.no -= (*itr).amount;
                    });
                }
                //add the vote
                auto itr_voted_proposals_a = std::find((*itr).agreed_proposal_ids.begin(), (*itr).agreed_proposal_ids.end(), id);
                if(itr_voted_proposals_a == (*itr).agreed_proposal_ids.end()) {
                    _voters.modify(itr, get_self(), [&](auto& v){
                        v.agreed_proposal_ids.emplace_back(id);
                    });
                    idx_index.modify(itr_proposal, get_self(), [&](auto& proposal){
                        proposal.yes += (*itr).amount;
                    });
                }
            }
            else{
                //delete existing vote on same proposal
                auto itr_voted_proposals_a = std::find((*itr).agreed_proposal_ids.begin(), (*itr).agreed_proposal_ids.end(), id);
                if(itr_voted_proposals_a != (*itr).agreed_proposal_ids.end()){
                    _voters.modify(itr, get_self(), [&](auto& v){
                        v.agreed_proposal_ids.erase(itr_voted_proposals_a);
                    });
                    idx_index.modify(itr_proposal, get_self(), [&](auto& proposal){
                        proposal.yes -= (*itr).amount;
                    });
                }
                //add the vote
                auto itr_voted_proposals_d = std::find((*itr).disagreed_proposal_ids.begin(), (*itr).disagreed_proposal_ids.end(), id);
                if(itr_voted_proposals_d == (*itr).disagreed_proposal_ids.end()){
                    _voters.modify(itr, get_self(), [&](auto& v){
                        v.disagreed_proposal_ids.emplace_back(id);
                    });
                    idx_index.modify(itr_proposal, get_self(), [&](auto& proposal){
                        proposal.no += (*itr).amount;
                    });
                }
            }
        }
    }

    void referendum_contract::refund(name voter){
        require_auth(voter);

        voters _voters(_self, _self.value);

        proposals _proposals(_self, _self.value);

        auto itr = _voters.find(voter.value);
        eosio_assert(itr != _voters.end(), "nothing to refund");

        for(uint64_t agreed_ids : (*itr).agreed_proposal_ids){
            auto idx_index = _proposals.get_index<name("idx")>();
            auto itr_proposal = idx_index.find(agreed_ids);
            idx_index.modify(itr_proposal, get_self(), [&](auto& proposal){
                proposal.yes -= (*itr).amount;
            });
        }

        for(uint64_t disagreed_ids : (*itr).disagreed_proposal_ids){
            auto idx_index = _proposals.get_index<name("idx")>();
            auto itr_proposal = idx_index.find(disagreed_ids);
            idx_index.modify(itr_proposal, get_self(), [&](auto& proposal){
                proposal.no -= (*itr).amount;
            });
        }

        eosio::action(
                eosio::permission_level{get_self() , name("active") },
                name("eosio.token"), name("transfer"),
                std::make_tuple( get_self(), voter, (*itr).tokens, std::string("Play price refund."))
        ).send();

        _voters.erase(itr);
    }
} //referendum

#define EOSIO_DISPATCH_CUSTOM(TYPE, MEMBERS) \
  extern "C" { \
  void apply(uint64_t receiver, uint64_t code, uint64_t action) { \
    auto self = receiver; \
    bool self_action = code == self && action != "transfer"_n.value; \
    bool transfer = action == "transfer"_n.value; \
    if (self_action || transfer) { \
      switch (action) { \
          EOSIO_DISPATCH_HELPER(TYPE, MEMBERS) \
      }    \
          /* does not allow destructor of this contract to run: eosio_exit(0); */ \
    } \
  } \
}

EOSIO_DISPATCH_CUSTOM(referendum::referendum_contract, (makeproposal)(transfer)(vote)(refund))