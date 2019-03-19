#include "referendum.hpp"

namespace referendum{

    void referendum_contract::makeproposal(name creator, const string& proposal_name, const string& content){
        require_auth(creator);
        eosio_assert(proposal_name.size() < 200, "proposal name is too long");
        eosio_assert(content.size() < 16384, "proposal content is too long");

        proposals _proposals(_self, _self.value);

        auto proposal_counter = _counter.get();

        _proposals.emplace(creator, [&](auto& proposal){
            proposal.id = proposal_counter.global_id;
            proposal.creator = creator;
            proposal.proposal_name = proposal_name;
            proposal.content = content;
            proposal.yes = 0;
            proposal.no = 0;
        });

        proposal_counter.global_id++;
        _counter.set(proposal_counter, _self);
    }

    void referendum_contract::transfer(uint64_t sender, uint64_t receiver){
        auto transfer_data = eosio::unpack_action_data<st_transfer>();

        if(transfer_data.from == get_self()){
            return;
        }

        eosio_assert(transfer_data.from == get_self() || transfer_data.to == get_self(), "Invalid transfer");

        eosio_assert(transfer_data.quantity.is_valid(), "Invalid asset");

        voters _voters(_self, _self.value);

        auto itr = _voters.find(voter.value);

        if(itr == _voters.end()){
            _voters.emplace(_self, [&](auto& voter){
                voter.name = sender;
                voter.amount = transfer_data.quantity.amount;
            });
        }
        else{
            _voters.modify(itr, 0, [&](auto& voter){
                voter.amount += transfer_data.quantity.amount;
            });
        }
    }

    void referendum_contract::vote(name voter, uint64_t id, bool is_agree){
        require_auth(voter);
        voters _voters(_self, _self.value);

        auto itr = _voters.find(voter.value);
        eosio_assert(itr != _voters.end(), "must transfer tokens first");

        auto idx_index = _proposals.get_index<name("idx")>();
        auto itr_proposal = idx_index.find(id);
        eosio_assert(itr_proposal != idx_index.end(), "Proposal not found");

        //auto itr_voted_proposals = std::find((*itr).proposal_ids.begin(), (*itr).proposal_ids.end(), id);

        proposals _proposals(_self, _self.value);

        _voters.modify(itr, 0, [&](auto& voter){

        });

        /*_proposals.modify(itr_proposal, 0, [&](auto& proposal){
            if(is_agree){
                proposal.yes += (*itr).amount;
            }
            else{
                proposal.no += (*itr).amount;
            }
        })*/
    }
} //referendum

EOSIO_DISPATCH(referendum::referendum_contract, (makeproposal)(transfer))