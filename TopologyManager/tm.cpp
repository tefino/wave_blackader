/*
 * Copyright (C) 2010-2011  George Parisis and Dirk Trossen
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of
 * the BSD license.
 *
 * See LICENSE and COPYING for more details.
 */

#include <signal.h>
#include <arpa/inet.h>
#include <set>
#include <blackadder.hpp>

#include "tm_igraph.hpp"

using namespace std;

Blackadder *ba;
TMIgraph tm_igraph;
pthread_t event_listener;

string req_id = "FFFFFFFFFFFFFFFE";
string req_prefix_id = string();
string req_bin_id = hex_to_chararray(req_id);
string req_bin_prefix_id = hex_to_chararray(req_prefix_id);

string resp_id = string();
string resp_prefix_id = "FFFFFFFFFFFFFFFD";
string resp_bin_id = hex_to_chararray(resp_id);
string resp_bin_prefix_id = hex_to_chararray(resp_prefix_id);

void handleRequest(char *request, int request_len) {
    unsigned char request_type;
    unsigned char no_publishers;
    unsigned char no_subscribers;
    string nodeID;
    set<string> publishers;
    set<string> subscribers;
    map<string, Bitvector *> result = map<string, Bitvector *>();
    map<string, Bitvector *>::iterator map_iter;
    unsigned char response_type;
    int idx = 0;
    unsigned char strategy;
    memcpy(&request_type, request, sizeof (request_type));
    memcpy(&strategy, request + sizeof (request_type), sizeof (strategy));
    if (request_type == MATCH_PUB_SUBS) {
        /*this a request for topology formation*/
        memcpy(&no_publishers, request + sizeof (request_type) + sizeof (strategy), sizeof (no_publishers));
        cout << "Publishers: ";
        for (int i = 0; i < (int) no_publishers; i++) {
            nodeID = string(request + sizeof (request_type) + sizeof (strategy) + sizeof (no_publishers) + idx, PURSUIT_ID_LEN);
            cout << nodeID << " ";
            idx += PURSUIT_ID_LEN;
            publishers.insert(nodeID);
        }
        cout << endl;
        cout << "Subscribers: ";
        memcpy(&no_subscribers, request + sizeof (request_type) + sizeof (strategy) + sizeof (no_publishers) + idx, sizeof (no_subscribers));
        for (int i = 0; i < (int) no_subscribers; i++) {
            nodeID = string(request + sizeof (request_type) + sizeof (strategy) + sizeof (no_publishers) + sizeof (no_subscribers) + idx, PURSUIT_ID_LEN);
            cout << nodeID << " ";
            idx += PURSUIT_ID_LEN;
            subscribers.insert(nodeID);
        }
        cout << endl;
        tm_igraph.calculateFID(publishers, subscribers, result);
        /*notify publishers*/
        for (map_iter = result.begin(); map_iter != result.end(); map_iter++) {
            if ((*map_iter).second == NULL) {
                cout << "Publisher " << (*map_iter).first << ", FID: NULL" << endl;
                response_type = STOP_PUBLISH;
                int response_size = request_len - sizeof(strategy) - sizeof (no_publishers) - no_publishers * PURSUIT_ID_LEN - sizeof (no_subscribers) - no_subscribers * PURSUIT_ID_LEN;
                char *response = (char *) malloc(response_size);
                memcpy(response, &response_type, sizeof (response_type));
                int ids_index = sizeof (request_type) + sizeof (strategy) + sizeof (no_publishers) + no_publishers * PURSUIT_ID_LEN + sizeof (no_subscribers) + no_subscribers * PURSUIT_ID_LEN;
                memcpy(response + sizeof (response_type), request + ids_index, request_len - ids_index);
                /*find the FID to the publisher*/
                string destination = (*map_iter).first;
                Bitvector *FID_to_publisher = tm_igraph.calculateFID(tm_igraph.nodeID, destination);
                string response_id = resp_bin_prefix_id + (*map_iter).first;
                ba->publish_data(response_id, IMPLICIT_RENDEZVOUS, (char *) FID_to_publisher->_data, FID_LEN, response, response_size);
                delete FID_to_publisher;
                free(response);
            } else {
                cout << "Publisher " << (*map_iter).first << ", FID: " << (*map_iter).second->to_string() << endl;
                response_type = START_PUBLISH;
                int response_size = request_len - sizeof(strategy) - sizeof (no_publishers) - no_publishers * PURSUIT_ID_LEN - sizeof (no_subscribers) - no_subscribers * PURSUIT_ID_LEN + FID_LEN;
                char *response = (char *) malloc(response_size);
                memcpy(response, &response_type, sizeof (response_type));
                int ids_index = sizeof (request_type) + sizeof (strategy) + sizeof (no_publishers) + no_publishers * PURSUIT_ID_LEN + sizeof (no_subscribers) + no_subscribers * PURSUIT_ID_LEN;
                memcpy(response + sizeof (response_type), request + ids_index, request_len - ids_index);
                memcpy(response + sizeof (response_type) + request_len - ids_index, (*map_iter).second->_data, FID_LEN);
                /*find the FID to the publisher*/
                string destination = (*map_iter).first;
                Bitvector *FID_to_publisher = tm_igraph.calculateFID(tm_igraph.nodeID, destination);
                string response_id = resp_bin_prefix_id + (*map_iter).first;
                ba->publish_data(response_id, IMPLICIT_RENDEZVOUS, (char *) FID_to_publisher->_data, FID_LEN, response, response_size);
                delete (*map_iter).second;
                delete FID_to_publisher;
                free(response);
            }
        }
    } else if ((request_type == SCOPE_PUBLISHED) || (request_type == SCOPE_UNPUBLISHED)) {
        /*this a request to notify subscribers about a new scope*/
        memcpy(&no_subscribers, request + sizeof (request_type) + sizeof (strategy), sizeof (no_subscribers));
        for (int i = 0; i < (int) no_subscribers; i++) {
            nodeID = string(request + sizeof (request_type) + sizeof (strategy) + sizeof (no_subscribers) + idx, PURSUIT_ID_LEN);
            Bitvector *FID_to_subscriber = tm_igraph.calculateFID(tm_igraph.nodeID, nodeID);
            int response_size = request_len - sizeof(strategy) - sizeof (no_subscribers) - no_subscribers * PURSUIT_ID_LEN + FID_LEN;
            int ids_index = sizeof (request_type) + sizeof (strategy) + sizeof (no_subscribers) + no_subscribers * PURSUIT_ID_LEN;
            char *response = (char *) malloc(response_size);
            string response_id = resp_bin_prefix_id + nodeID;
            memcpy(response, &request_type, sizeof (request_type));
            memcpy(response + sizeof (request_type), request + ids_index, request_len - ids_index);
            //cout << "PUBLISHING NOTIFICATION ABOUT NEW OR DELETED SCOPE to node " << nodeID << " using FID " << FID_to_subscriber->to_string() << endl;
            ba->publish_data(response_id, IMPLICIT_RENDEZVOUS, FID_to_subscriber->_data, FID_LEN, response, response_size);
            idx += PURSUIT_ID_LEN;
            delete FID_to_subscriber;
            free(response);
        }
	} else if (request_type == WAVE_NOTIFY_SUB) {
		cout<<"receive WAVE_NOTIFY_SUB"<<endl ;
		memcpy(&no_publishers, request+sizeof(request_type), sizeof(no_publishers)) ;
		idx = 0 ;
		publishers.clear() ;
		for (int i = 0; i < (int) no_publishers; i++) {
			nodeID = string(request + sizeof (request_type) + sizeof (no_publishers) + idx, PURSUIT_ID_LEN);
			idx += PURSUIT_ID_LEN;
			publishers.insert(nodeID);
		}
		string sub ;
		sub = string(request + sizeof (request_type) + sizeof (no_publishers) + idx, NODEID_LEN) ;
		Bitvector fid2pub(FID_LEN*8) ;
		tm_igraph.calculateFID(publishers, sub, fid2pub) ;
		char* response ;
		unsigned int response_len = request_len - sizeof(no_publishers) - idx - NODEID_LEN + FID_LEN ;
		response = (char*) malloc(response_len) ;
		memcpy(response, &request_type, sizeof(request_type)) ;
		memcpy(response+sizeof(request_type), fid2pub._data, FID_LEN) ;
		memcpy(response+sizeof(request_type)+FID_LEN, request + sizeof (request_type) + sizeof (no_publishers) + idx + NODEID_LEN,\
			   request_len-(sizeof (request_type) + sizeof (no_publishers) + idx + NODEID_LEN)) ;
		Bitvector* fid2sub = tm_igraph.calculateFID(tm_igraph.nodeID, sub) ;
		string response_id = resp_bin_prefix_id + sub;
		ba->publish_data(response_id, IMPLICIT_RENDEZVOUS, fid2sub._data, FID_LEN, response, response_len);
		delete fid2sub ;
		free(response) ;
	}
}

void *event_listener_loop(void *arg) {
    Blackadder *ba = (Blackadder *) arg;
    while (true) {
        Event ev;
        ba->getEvent(ev);
        if (ev.type == PUBLISHED_DATA) {
            //cout << "TM: received a request...processing now" << endl;
            handleRequest((char *) ev.data, ev.data_len);
        } else {
            cout << "TM: I am not expecting any other notification...FATAL" << endl;
        }
    }
}

void sigfun(int sig) {
    (void) signal(SIGINT, SIG_DFL);
    cout << "TM: disconnecting" << endl;
    ba->disconnect();
    delete ba;
    cout << "TM: exiting" << endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    (void) signal(SIGINT, sigfun);
    cout << "TM: starting - process ID: " << getpid() << endl;
    if (argc != 2) {
        cout << "TM: the topology file is missing" << endl;
        exit(0);
    }
    /*read the graphML file that describes the topology*/
    if (tm_igraph.readTopology(argv[1]) < 0) {
        cout << "TM: couldn't read topology file...aborting" << endl;
        exit(0);
    }
    cout << "Blackadder Node: " << tm_igraph.nodeID << endl;
    /***************************************************/
    if (tm_igraph.mode.compare("kernel") == 0) {
        ba = Blackadder::Instance(false);
    } else {
        ba = Blackadder::Instance(true);
    }
    pthread_create(&event_listener, NULL, event_listener_loop, (void *) ba);
    ba->subscribe_scope(req_bin_id, req_bin_prefix_id, IMPLICIT_RENDEZVOUS, NULL, 0);

    pthread_join(event_listener, NULL);
    cout << "TM: disconnecting" << endl;
    ba->disconnect();
    delete ba;
    cout << "TM: exiting" << endl;
    return 0;
}
