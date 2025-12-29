#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <execution>
#include <algorithm>
#include <mutex>
#include <ApplicationServices/ApplicationServices.h>
#include "state.h"

#define NUM_THREADS 1

using namespace std;

struct Node {
    State * state;
    Node *parent;
    unordered_map<long long, Node *> children;
    int visits;
    double reward;
    bool isChance;
    mutex childMutex;
    Node(State * state, Node * parent, bool isChance){
        this->state = state;
        this->parent = parent;
        this->visits = 0;
        this->reward = 0;
        this->isChance = isChance;

        if (!isChance){
            //TODO: pass by reference
            vector<State *> availableStates = state->getAvailableStates();

            for(int i=0;i<availableStates.size();i++){
                Node *child = new Node(availableStates[i], this, true);
                addChild(child);
            }
        } else {
            // Chance node children are added in the selection phase.
        }
    }

    // destructor
    ~Node(){
        for(auto it=children.begin();it!=children.end();it++){
            delete it->second;
            it->second = NULL;
        }
        delete state;
        state = NULL;
    }

    void addChild(Node *child) {
        children[child->hash_code()] = child;
    }
    void addReward(double reward) {
        this->reward += reward;
    }
    void addVisit() {
        this->visits++;
    }
    double getUCB1() {
        if (this->visits == 0) {
            return 1000000000;
        }
        return (double)reward/(double)this->visits + sqrt(2*log(parent->visits)/(double)visits);
    }
    Node* getBestChild() {
        Node *bestChild = NULL;
        double bestUCB1 = -10000;
        for(auto it=children.begin();it!=children.end();it++){
            Node *child = it->second;
            double UCB1 = child->getUCB1();
            if (UCB1 > bestUCB1){
                bestUCB1 = UCB1;
                bestChild = child;
            }
            if (UCB1 == 1000000000){
                return child;
            }
        }
        return bestChild;
    }
    Node* getBestEVChild() {
        Node *bestChild = NULL;
        double bestEV = -10000;
        for(auto it=children.begin();it!=children.end();it++){
            Node *child = it->second;
            double EV = (double)child->reward/(double)child->visits;
            if (EV > bestEV){
                bestEV = EV;
                bestChild = child;
            }
        }
        return bestChild;
    }
    Node * getMostVisitedChild(){
        Node *bestChild = NULL;
        int bestVisits = -1;
        for(auto it=children.begin();it!=children.end();it++){
            Node *child = it->second;
            int visits = child->visits;
            if (visits > bestVisits){
                bestVisits = visits;
                bestChild = child;
            }
        }
        return bestChild;
    }
    bool isLeaf() {
        return children.size()==0;
    }
    void print() {
        state->print();
        cout<<"Visits: "<<visits<<endl;
        cout<<"Reward: "<<reward<<endl;
        //cout<<"UCB1: "<<getUCB1()<<endl;
        cout<<"EV: "<<(double)reward/(double)visits<<endl;
    }
    long long hash_code(){
        return state->hash_code();
    }
};
void mctsTask(Node * rootNode, int iters, int threadid = 0){
    double initScore = rootNode->state->score;
    double cardsLeft = rootNode->state->cardsLeft;
    if (cardsLeft == 0){
        cardsLeft = 1;
    }
    double simPoints = 0;
    int simCnt = 0;
    for(int i = 0; i < iters; i++){
        Node *node = rootNode;

        // Selection and expansion
        while(true){
            // node->print();
            // cout << "is chance: " << node->isChance << endl;
            // getchar();
            if (node->isChance){
                State * state = node->state->sampleState();

                Node * lockingNode = node;
                long long hc = state->hash_code();
                bool exists = node->children.count(hc);
                if (!exists){
                    Node *child = new Node(state, node, false);
                    node->addChild(child);
                    node = child;
                } else {
                    node = node->children[hc];
                    delete state;
                }
            } else {
                if (node->isLeaf()){
                    break;
                }
                node = node->getBestChild();

                if (node->visits == 0){
                    break;
                }
            }
        }
        State * simState = node->state->sampleState();
        bool shouldPrint = simState->cardsLeft >= 50;
        
        double reward = 0;
        double scaleFactor = 100 * cardsLeft / 51;

        while (!simState->isTerminal()){
            //simState->print();
            //getchar();
            if (!simState->makeSmartMove()){
                break;
            }
            State * oldState = simState;
            simState = simState->sampleState();
            delete oldState;
        }

        reward = (simState->score - simState->numUndo / 5 - initScore - scaleFactor) / scaleFactor;

        if (shouldPrint){
            simPoints += simState->score;
            simCnt++;
        }

        // Backpropagation
        while(node!=NULL){
            node->addReward(reward);
            node->addVisit();
            node = node->parent;
        }

        delete simState;
    }
    cout << "simulation quality = " << simPoints / simCnt << endl;
    getchar();
}

Node* MCTS(State * state, int iterations) {
    Node **roots = new Node *[NUM_THREADS];
    for(int i = 0; i < NUM_THREADS; i++){
        if (i == 0){
            roots[i] = new Node(state, NULL, false);
        } else {
            roots[i] = new Node(new State(state), NULL, false);
        }
    }
    thread t[NUM_THREADS];
    int iters = iterations / NUM_THREADS;
    for(int i=0;i<NUM_THREADS;i++){
        t[i] = thread(mctsTask, roots[i], iters, i);
    }
    for(int i=0;i<NUM_THREADS;i++){
        t[i].join();
    }
    unordered_map<long long, int> visiteds;

    for(int t = 0; t < NUM_THREADS; t++){
        Node *root = roots[t];
        for(auto it=root->children.begin();it!=root->children.end();it++){
            Node *child = it->second;
            int visits = child->visits;
            visiteds[it->first];
            visiteds[it->first] += visits;
            //cout << it->first << " " << visits << endl;
            //getchar();
        }
    }

    Node * bestChild;
    int bestVisits = -1;
    for(auto it=roots[0]->children.begin();it!=roots[0]->children.end();it++){
        Node *child = it->second;
        long long hc = it->first;
        if (visiteds[hc] > bestVisits){
            bestVisits = visiteds[hc];
            bestChild = child;
        }
    }
    for(int i = 1; i < NUM_THREADS; i++){
        delete roots[i];
    }

    return bestChild;
}

State * sampleFromScreenshot(State * state, int prevCard){
    while(true){
        CGImageRef img = CGDisplayCreateImage(CGMainDisplayID()); 
        CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(img));

        int width = CGImageGetWidth(img);
        int height = CGImageGetHeight(img);
        int bpp = CGImageGetBitsPerPixel(img) / 8;

        unsigned char *pixels = (unsigned char *)CFDataGetBytePtr(data);

        State * ret = state->fromPixels(pixels, width, height, bpp, prevCard);
        this_thread::sleep_for(chrono::milliseconds(200));

        CGImageRef img2 = CGDisplayCreateImage(CGMainDisplayID());
        CFDataRef data2 = CGDataProviderCopyData(CGImageGetDataProvider(img2));

        unsigned char *pixels2 = (unsigned char *)CFDataGetBytePtr(data2);
        State * ret2 = state->fromPixels(pixels2, width, height, bpp, prevCard);

        CFRelease(data);
        CGImageRelease(img);
        CFRelease(data2);
        CGImageRelease(img2);
        if (ret == NULL){
            this_thread::sleep_for(chrono::milliseconds(100));
        } else {
            if (ret2 == NULL || ret2->curRank != ret->curRank || ret2->curSuit != ret->curSuit){
                delete ret2;
                delete ret;
                continue;
            }
            delete ret2;

            cout << "detected " << ret->curRank << " " << ret->curSuit << endl;
            return ret;
        }
    }
    return NULL;
}

// bad case
// totals: 9 0 0 11 
// numCards: 1 0 0 2 
// left: 0 0 1 0 0 1 1 0 0 0 1 
// curCard: 10, hold = -1
// cardsLeft: 4, score: 174, usedHold: 0, holdsLeft: 0, hasBusted: 0, curMove: -1, 
// 99070/100000
// totals: 9 0 0 0 
// numCards: 1 0 0 0 
// left: 0 0 1 0 0 1 1 0 0 0 0 
// curCard: 10, hold = -1
// cardsLeft: 3, score: 184, usedHold: 0, holdsLeft: 0, hasBusted: 0, curMove: -1, 
// 99429/100000


void test1(){
    // totals: 11 11 11 11 
    // numCards: 2 2 2 2 
    // left: 2 3 2 3 2 3 3 2 2 1 13 
    // curCard: 8, hold = -1
    // cardsLeft: 36, score: 17, streak: 0, justUndid: 0, canUndo: 0, lastPos: 0, nextCard: 10, nextNextCard: -1, usedHold: 0, holdsLeft: 0, hasBusted: 0, curMove: -1, 

    State * state = new State();
    state->totals[0] = 11;
    state->totals[1] = 11;
    state->totals[2] = 11;
    state->totals[3] = 11;
    state->numCards[0] = 2;
    state->numCards[1] = 2;
    state->numCards[2] = 2;
    state->numCards[3] = 2;
    state->left[0] = 2;
    state->left[1] = 3;
    state->left[2] = 2;
    state->left[3] = 3;
    state->left[4] = 2;
    state->left[5] = 3;
    state->left[6] = 3;
    state->left[7] = 2;
    state->left[8] = 2;
    state->left[9] = 1;
    state->left[10] = 13;
    state->curCard = 8;
    state->cardsLeft = 36;
    state->score = 17;
    state->nextCard = 10;
    state->nextNextCard = -1;
    state->holdsLeft = 0;
    state->curMove = -1;
    state->canUndo = false;
    state->lastPos = 0;
    state->prevCard = 6;

    Node *root = MCTS(state, 100000);
    cerr << "done" << endl;
    cerr << root << endl;
    root->state->print();
}

void test2(){
    // totals: 11 0 0 0 
    // numCards: 2 0 0 0 
    // soft: 0 0 0 0 
    // left: 0 1 0 0 0 0 0 0 0 1 2 
    // curCard: 6, hold = -1
    // cardsLeft: 4, score: 203, streak: 3, justUndid: 0, canUndo: 0, lastPos: 2, nextCard: -1, nextNextCard: -1, usedHold: 0, holdsLeft: 2, hasBusted: 0, curMove: -1,
}

void simulate(){
    double cumScore = 0;
    int numGames = 0;
    srand(time(NULL));
    while(true){
        numGames++;
        State * state = new State();
        State * initState = state;
        State * sampled = state->sampleState();
        state = sampled;

        vector<Node *> toDelete;
        while(true){
            state->print();
            //getchar();
            if(state == NULL){
                cerr << "NULL STATE wtf" << endl;
            }
            if (state->isTerminal()){
                break;
            }
            Node *root = MCTS(state, 200000);
            state = root->state;
            State * sampled = state->sampleState();

            state = sampled;
            cout << root->visits << "/" << root->parent->visits << endl;
            delete root->parent;
        }

        cumScore += state->score;
        cout << "score = " << state->score << endl;
        cout << "average score: " << cumScore / numGames << endl;
        cout << "num games: " << numGames << endl;
        delete initState;
        delete state;
    }
}


int main() {
    //readClearable();
    //calcClearable(false);
    //writeClearable();
    simulate();
    return 0;
    srand(time(NULL));
    long long hashcode = -1;
    bool printed = false;
    while(true){
        State * state = new State();
        Node * root = NULL;
        int prevCard = -1;
        while(true){
            State * newState = sampleFromScreenshot(root == NULL ? state : root->state, prevCard);
            newState->print();

            prevCard = newState->curRank * 4 + newState->curSuit;
            if (root != NULL){
                delete root->parent;
            } else {
                delete state;
            }
            state = newState;

            if (state->isTerminal()){
                break;
            }

            root = MCTS(state, 100000);
            cout << "best move: ";
            if (root->state->justHeld){
                cout << "hold" << endl;
            } else if (root->state->usedHold){
                cout << "use hold" << endl;
            } else if (root->state->justUndid){
                cout << "undo" << endl;
            } else {
                cout << root->state->curMove << endl;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));

    }
    return 0;
}