/*
#include "listofmoves.h"

namespace listofmoves {

    struct node {
        std::string move;      // move counter
        listOfMoves next;
    }; 


    listOfMoves createEmptyList() {
        listOfMoves movelist;
        movelist = nullptr;
        return movelist;
    }

    void addToList(listOfMoves movelist, std::string moveToAdd) {        
        if (movelist == nullptr) {
            movelist->move = moveToAdd;
            movelist->next = nullptr;
            return;
        }

        node *temp = new node;
        temp->move = moveToAdd;
        temp->next = nullptr;
        
        listOfMoves cursor = movelist;
        while (cursor->next != nullptr) cursor = cursor->next;
        cursor->next = temp;
            
        
    }

}
*/