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
    double squaredReward;
    bool isChance;
    mutex childMutex;
    Node(State * state, Node * parent, bool isChance){
        this->state = state;
        this->parent = parent;
        this->visits = 0;
        this->reward = 0;
        this->squaredReward = 0;
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
        this->squaredReward += reward*reward;
    }
    void addVisit() {
        this->visits++;
    }
    double getUCB1() {
        if (this->visits <= 0) {
            return 1000000000;
        }
        double mean = (double)reward/(double)visits;
        return mean + sqrt(2*log(parent->visits)/(double)visits)
            + sqrt((squaredReward - mean * mean * visits + 20) / (double)visits);
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
            //cout << child->state->curMove << ":" << UCB1 << ",";
            if (UCB1 == 1000000000){
                return child;
            }
        }
        //cout << endl;
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
    Node* getMostVisitedChild() {
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
pair<double,double> mctsTask(Node * rootNode, int iters, double b = 0, double d = 1){
    double initScore = rootNode->state->score;
    double cardsLeft = rootNode->state->cardsLeft;
    if (cardsLeft == 0){
        cardsLeft = 1;
    }
    if (d == 0) d = 1;
    double simPoints = 0;
    int simCnt = 0;
    int minScore = 999;
    int maxScore = 0;
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

                // cout << "iter: " << i << ", " << node->children.size() << endl;
                // node->print();
                Node * parentNode = node;
                node = node->getBestChild();
                // cout << "best move: " << node->state->curMove << endl;
                // getchar();

                if (node->visits <= 0 && parentNode->children.size() > 1){
                    break;
                }
            }
        }
        State * simState = node->state->sampleState();
        bool shouldPrint = simState->cardsLeft >= 50;
        
        double reward = 0;
        double scaleFactor = 100 * cardsLeft / 51;

        while (!simState->isTerminal()){
            // simState->print();
            // getchar();
            if (!simState->makeSmartMove()){
                break;
            }
            State * oldState = simState;
            simState = simState->sampleState();
            delete oldState;
        }

        minScore = min(minScore, simState->score - simState->numUndo / 5);
        maxScore = max(maxScore, simState->score - simState->numUndo / 5);
    
        if (b > 0){
            reward = (simState->score - simState->numUndo / 5 - b) / d;
        } else {
            reward = (simState->score - simState->numUndo / 5 - initScore - scaleFactor) / scaleFactor;
        }


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
    //cout << "simulation quality = " << simPoints / simCnt << endl;
    //getchar();
    return make_pair(minScore, maxScore);
}

Node* MCTS(State * state, int iterations) {
    Node *copy = new Node(new State(state), NULL, false);
    auto minmax = mctsTask(copy, iterations / 100);
    delete copy;
    Node *root = new Node(state, NULL, false);
    thread t[NUM_THREADS];
    int iters = iterations / NUM_THREADS;
    for(int i=0;i<NUM_THREADS;i++){
        t[i] = thread(mctsTask, root, iters, (minmax.second - minmax.first) / 2 + minmax.first, (minmax.second - minmax.first) / 2);
    }
    for(int i=0;i<NUM_THREADS;i++){
        t[i].join();
    }
    return root->getMostVisitedChild();
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
// curCard: 10
// cardsLeft: 4, score: 174, hasBusted: 0, curMove: -1, 
// 99070/100000
// totals: 9 0 0 0 
// numCards: 1 0 0 0 
// left: 0 0 1 0 0 1 1 0 0 0 0 
// curCard: 10
// cardsLeft: 3, score: 184, hasBusted: 0, curMove: -1, 
// 99429/100000


void test1(){
    // totals: 11 11 11 11 
    // numCards: 2 2 2 2 
    // left: 2 3 2 3 2 3 3 2 2 1 13 
    // curCard: 8
    // cardsLeft: 36, score: 17, streak: 0, justUndid: 0, canUndo: 0, lastPos: 0, nextCard: 10, nextNextCard: -1, hasBusted: 0, curMove: -1, 

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
    // curCard: 6
    // cardsLeft: 4, score: 203, streak: 3, justUndid: 0, canUndo: 0, lastPos: 2, nextCard: -1, nextNextCard: -1, hasBusted: 0, curMove: -1,
}

void test3(){
    // totals: 12 0 0 0 
    // numCards: 3 0 0 0 
    // soft: 0 0 0 0 
    // left: 0 0 1 0 0 0 0 1 0 0 0 
    // curCard: 4
    // cardsLeft: 2, score: 213, streak: 3, justUndid: 0, canUndo: 0, lastPos: 3, nextCard: -1, nextNextCard: -1, hasBusted: 0, curMove: -1, prevCard: 1, undoCounter: 1, numUndo: 1, 
    State * state = new State();
    state->totals[0] = 12;
    state->numCards[0] = 3;
    state->left[0] = 0;
    state->left[1] = 0;
    state->left[2] = 1;
    state->left[3] = 0;
    state->left[4] = 0;
    state->left[5] = 0;
    state->left[6] = 0;
    state->left[7] = 1;
    state->left[8] = 0;
    state->left[9] = 0;
    state->left[10] = 0;
    state->curCard = 4;
    state->cardsLeft = 2;
    state->score = 213;
    state->streak = 3;
    state->nextCard = -1;
    state->nextNextCard = -1;
    state->curMove = -1;
    state->canUndo = false;
    state->lastPos = 3;
    state->prevCard = 1;
    state->undoCounter = 1;
    state->numUndo = 1;

    Node *root = MCTS(state, 100000);
    cerr << "done" << endl;
    cerr << root << endl;
    root->state->print();

            cout << root->visits << "/" << root->parent->visits << endl;
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
            Node *root = MCTS(state, 1000);
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
    //test3();
    //return 0;
    //readClearable();
    //calcClearable(false);
    //writeClearable();
    // simulate();
    // return 0;
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
            if (root->state->justUndid){
                cout << "undo" << endl;
            } else {
                cout << root->state->curMove << endl;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));

    }
    return 0;
}