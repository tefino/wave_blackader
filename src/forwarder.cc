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

#include "forwarder.hh"

CLICK_DECLS

ForwardingEntry::ForwardingEntry() {
    src = NULL;
    dst = NULL;
    src_ip = NULL;
    dst_ip = NULL;
    LID = NULL;
	interface_no = String() ;
}

ForwardingEntry::~ForwardingEntry() {
    if (src != NULL) {
        delete src;
    }
    if (dst != NULL) {
        delete dst;
    }
    if (src_ip != NULL) {
        delete src_ip;
    }
    if (dst_ip != NULL) {
        delete dst_ip;
    }
    if (LID != NULL) {
        delete LID;
    }
}

Forwarder::Forwarder() {

}

Forwarder::~Forwarder() {
    click_chatter("Forwarder: destroyed!");
}

int Forwarder::configure(Vector<String> &conf, ErrorHandler *errh) {
    int port;
    int reverse_proto;
	int reverse_subtype ;
	int reverse_datatype ;
    gc = (GlobalConf *) cp_element(conf[0], this);
    _id = 0;
    click_chatter("*****************************************************FORWARDER CONFIGURATION*****************************************************");
    click_chatter("Forwarder: internal LID: %s", gc->iLID.to_string().c_str());

    cp_integer(String("0x080a"), 16, &reverse_proto);
	cp_integer(String("0x080b"), 16, &reverse_subtype) ;
	cp_integer(String("0x080c"), 16, &reverse_datatype) ;
    proto_type = htons(reverse_proto);
	sub_type = htons(reverse_subtype) ;
	data_type = htons(reverse_datatype) ;
    if (gc->use_mac == true) {
        cp_integer(conf[1], &total_cache_size) ;
        cp_integer(conf[2], &number_of_links);
        click_chatter("Forwarder: Number of Links: %d", number_of_links);
        for (int i = 0; i < number_of_links; i++) {
            cp_integer(conf[3 + 4 * i], &port);
            EtherAddress * src = new EtherAddress();
            EtherAddress * dst = new EtherAddress();
            cp_ethernet_address(conf[4 + 4 * i], src, this);
            cp_ethernet_address(conf[5 + 4 * i], dst, this);
            ForwardingEntry *fe = new ForwardingEntry();
            fe->src = src;
            fe->dst = dst;
            fe->port = port;
			fe->interface_no += (unsigned char) i ;
            fe->LID = new BABitvector(FID_LEN * 8);
            for (int j = 0; j < conf[6 + 4 * i].length(); j++) {
                if (conf[6 + 4 * i].at(j) == '1') {
                    (*fe->LID)[conf[6+ 4 * i].length() - j - 1] = true;
                } else {
                    (*fe->LID)[conf[6 + 4 * i].length() - j - 1] = false;
                }
            }
            fwTable.push_back(fe);
            if (port != 0) {
                click_chatter("Forwarder: Added forwarding entry: port %d - source MAC: %s - destination MAC: %s - LID: %s", fe->port, fe->src->unparse().c_str(), fe->dst->unparse().c_str(), fe->LID->to_string().c_str());
            } else {
                click_chatter("Forwarder: Added forwarding entry for the internal LINK ID: %s", fe->LID->to_string().c_str());
            }
        }
    } else {
        cp_integer(conf[1], &number_of_links);
        click_chatter("Forwarder: Number of Links: %d", number_of_links);
        for (int i = 0; i < number_of_links; i++) {
            cp_integer(conf[2 + 4 * i], &port);
            IPAddress * src_ip = new IPAddress();
            IPAddress * dst_ip = new IPAddress();
            cp_ip_address(conf[3 + 4 * i], src_ip, this);
            cp_ip_address(conf[4 + 4 * i], dst_ip, this);
            ForwardingEntry *fe = new ForwardingEntry();
            fe->src_ip = src_ip;
            fe->dst_ip = dst_ip;
            fe->port = port;
            fe->LID = new BABitvector(FID_LEN * 8);
            for (int j = 0; j < conf[5 + 4 * i].length(); j++) {
                if (conf[5 + 4 * i].at(j) == '1') {
                    (*fe->LID)[conf[5 + 4 * i].length() - j - 1] = true;
                } else {
                    (*fe->LID)[conf[5 + 4 * i].length() - j - 1] = false;
                }
            }
            fwTable.push_back(fe);
            click_chatter("Forwarder: Added forwarding entry: port %d - source IP: %s - destination IP: %s - LID: %s", fe->port, fe->src_ip->unparse().c_str(), fe->dst_ip->unparse().c_str(), fe->LID->to_string().c_str());
        }
    }
    click_chatter("*********************************************************************************************************************************");
    //click_chatter("Forwarder: Configured!");
    return 0;
}

int Forwarder::initialize(ErrorHandler *errh) {
    //click_chatter("Forwarder: Initialized!");
	data_sent_byte = 0 ;
	data_sent_GB = 0 ;
	data_forward_byte = 0 ;
	data_forward_GB = 0 ;
	oneGB = 1073741824 ;

	current_cache_size = 0 ;
    Billion = 1000000000 ;
    cache_hit = 0 ;
    cache_hit_Bill = 0 ;
    cache_replace = 0 ;
    cache_replace_Bill = 0 ;
    return 0;
}

void Forwarder::cleanup(CleanupStage stage) {
    if (stage >= CLEANUP_CONFIGURED) {
        for (int i = 0; i < number_of_links; i++) {
            ForwardingEntry *fe = fwTable.at(i);
            delete fe;
        }
    }
	FILE *ft ;
	if( (ft = fopen("/home/wave_forwarder.dat", "w+")) == NULL )
		click_chatter("forwarder fopen error") ;
	fprintf(ft, "data_sent_byte: %d\ndata_sent_GB: %d\ndata_forward_byte: %d\ndata_forward_GB: %d\n",
		data_sent_byte, data_sent_GB, data_forward_byte, data_forward_GB) ;
	fclose(ft) ;
    if( (ft = fopen("/home/wave_cacheunit.dat", "w+")) == NULL )
        click_chatter("cacheunit fopen error");
    fprintf(ft, "cache_hit: %d\ncache_hit_Bill: %d\ncache_replace: %d\ncache_replace_Bill: %d\n",
            cache_hit, cache_hit_Bill, cache_replace, cache_replace_Bill) ;
    fprintf(ft, "total_cache_number_chunk: %d\n", cache.size()) ;
    for( int i = 0 ; i < cache.size() ; i++)
    {
        fprintf(ft, "%s\n", cache[i]->cached_chunks[0].quoted_hex().c_str()) ;
    }
	click_chatter("Forwarder: Cleaned Up!");
}

void Forwarder::push(int in_port, Packet *p) {
    WritablePacket *newPacket;
    WritablePacket *payload = NULL;
    ForwardingEntry *fe;
    Vector<ForwardingEntry *> out_links;
    BABitvector FID(FID_LEN * 8);
    BABitvector andVector(FID_LEN * 8);
    Vector<ForwardingEntry *>::iterator out_links_it;
    int counter = 1;
    bool pushLocally = false;
    click_ip *ip;
    click_udp *udp;
    if (in_port == 0 || in_port == 2) {
        memcpy(FID._data, p->data(), FID_LEN);
        /*Check all entries in my forwarding table and forward appropriately*/
        for (int i = 0; i < fwTable.size(); i++) {
            fe = fwTable[i];
            andVector = (FID)&(*fe->LID);
            if (andVector == (*fe->LID)) {
                out_links.push_back(fe);
            }
        }
        if (out_links.size() == 0) {
            /*I can get here when an app or a click element did publish_data with a specific FID
             *Note that I never check if I can push back the packet above if it matches my iLID
             * the upper elements should check before pushing*/
            p->kill();
        }
        for (out_links_it = out_links.begin(); out_links_it != out_links.end(); out_links_it++) {
            if (counter == out_links.size()) {
                payload = p->uniqueify();
            } else {
                payload = p->clone()->uniqueify();
            }
            fe = *out_links_it;
            if (gc->use_mac) {
                newPacket = payload->push_mac_header(14);
                /*prepare the mac header*/
                /*destination MAC*/
                memcpy(newPacket->data(), fe->dst->data(), MAC_LEN);
                /*source MAC*/
                memcpy(newPacket->data() + MAC_LEN, fe->src->data(), MAC_LEN);
				if(in_port == 0){
					/*protocol type 0x080a*/
					memcpy(newPacket->data() + MAC_LEN + MAC_LEN, &proto_type, 2);
				}
				if(in_port == 2){
					/*protocol type 0x080b*/
					memcpy(newPacket->data() + MAC_LEN + MAC_LEN, &sub_type, 2);
				}
                /*push the packet to the appropriate ToDevice Element*/
				data_sent_byte += newPacket->length() ;
				if( data_sent_byte >= oneGB ){
					data_sent_byte = data_sent_byte - oneGB ;
					data_sent_GB++ ;
				}
                output(fe->port).push(newPacket);
            } else {
                newPacket = payload->push(sizeof (click_udp) + sizeof (click_ip));
                ip = reinterpret_cast<click_ip *> (newPacket->data());
                udp = reinterpret_cast<click_udp *> (ip + 1);
                // set up IP header
                ip->ip_v = 4;
                ip->ip_hl = sizeof (click_ip) >> 2;
                ip->ip_len = htons(newPacket->length());
                ip->ip_id = htons(_id.fetch_and_add(1));
                ip->ip_p = IP_PROTO_UDP;
                ip->ip_src = fe->src_ip->in_addr();
                ip->ip_dst = fe->dst_ip->in_addr();
                ip->ip_tos = 0;
                ip->ip_off = 0;
                ip->ip_ttl = 250;
                ip->ip_sum = 0;
                ip->ip_sum = click_in_cksum((unsigned char *) ip, sizeof (click_ip));
                newPacket->set_ip_header(ip, sizeof (click_ip));
                // set up UDP header
                udp->uh_sport = htons(55555);
                udp->uh_dport = htons(55555);
                uint16_t len = newPacket->length() - sizeof (click_ip);
                udp->uh_ulen = htons(len);
                udp->uh_sum = 0;
                unsigned csum = click_in_cksum((unsigned char *) udp, len);
                udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
                output(fe->port).push(newPacket);
            }
            counter++;
        }
	}else if (in_port == 3){
		/**a packet has been pushed by the underlying network.**/
		/*check if it needs to be forwarded*/
		if (gc->use_mac) {
			memcpy(FID._data, p->data() + 14, FID_LEN);
		}
		String fileID ;
		String chunkID ;
		unsigned int totalchunkno = 0 ;
		unsigned int chunkno = 0 ;
		unsigned char idlen = 0 ;
		BABitvector testFID(FID) ;
		BABitvector reverse_FID(FID_LEN*8) ;//WAVE: modify the 2sub fid
		EtherAddress reverse_dst ;//WAVE: modify the 2sub fid
		EtherAddress reverse_src ;//WAVE: modify the 2sub fid
		memcpy(reverse_src.data(), p->data(), MAC_LEN) ;
		memcpy(reverse_dst.data(), p->data()+MAC_LEN, MAC_LEN) ;
		memcpy(reverse_FID._data, p->data()+14+FID_LEN+sizeof(unsigned char), FID_LEN) ;
		idlen = *(p->data()+14+FID_LEN+sizeof(unsigned char)+FID_LEN) ;
		fileID = String((const char*)(p->data()+14+FID_LEN+sizeof(unsigned char)+FID_LEN+sizeof(idlen)), idlen*PURSUIT_ID_LEN) ;
		chunkID = String((const char*)(p->data()+14+FID_LEN+sizeof(unsigned char)+FID_LEN+sizeof(idlen))+idlen*PURSUIT_ID_LEN, PURSUIT_ID_LEN) ;
		memcpy(&chunkno, p->data()+14+FID_LEN+sizeof(unsigned char)+FID_LEN+sizeof(idlen)+idlen*PURSUIT_ID_LEN+PURSUIT_ID_LEN, sizeof(chunkno)) ;
		memcpy(&totalchunkno, p->data()+14+FID_LEN+sizeof(unsigned char)+FID_LEN+sizeof(idlen)+idlen*PURSUIT_ID_LEN+PURSUIT_ID_LEN+sizeof(chunkno), sizeof(totalchunkno)) ;

		testFID.negate();
		if (!testFID.zero()) {
			/*Check all entries in my forwarding table and forward appropriately*/
			for (int i = 0; i < fwTable.size(); i++) {
				fe = fwTable[i];
				andVector = (FID)&(*fe->LID);
				if (andVector == (*fe->LID)) {
					EtherAddress src(p->data() + MAC_LEN);
					EtherAddress dst(p->data());
					if ((src.unparse().compare(fe->dst->unparse()) == 0) && (dst.unparse().compare(fe->src->unparse()) == 0)) {
						click_chatter("MAC: a loop from positive..I am not forwarding to the interface I received the packet from");
						continue;
					}
					out_links.push_back(fe);
				}
				//WAVE: find the reverse fid
				if(((*fe->src) == reverse_src) && ((*fe->dst) == reverse_dst))
				{
					reverse_FID |= (*fe->LID) ;
				}
			}
		} else {
			/*all bits were 1 - probably from a link_broadcast strategy--do not forward*/
		}
		/*check if the packet must be pushed locally*/
		andVector = FID & gc->iLID;
		if (andVector == gc->iLID) {
			if (gc->use_mac) {
				p->pull(14 + FID_LEN);
				payload = p->uniqueify() ;
				memcpy(payload->data()+1, reverse_FID._data, FID_LEN) ;
				output(2).push(payload);
			}
			return ;
		}
		if(out_links.size() != 1)
		{
			click_chatter("more than one out interface") ;
			p->kill() ;
			return ;
		}
		fe = out_links[0] ;
		
		for(Vector<CacheEntry*>::iterator iter_cache = cache.begin() ; iter_cache != cache.end() ; iter_cache++)
		{
			if( (*iter_cache)->match_filechunk(fileID, chunkID) )
			{
				unsigned char cachesetflag = 0 ;
				//update cache information according to the algorithm
				//and send back the data
				int upperbound = 0 ;
				for( int i = 0 ; i <= (*iter_cache)->interface_state[fe->interface_no] ; i++ )
				{
					upperbound += (int) pow((float) ((*iter_cache)->CMW_base), (float) i) ;
				}
				if( (*iter_cache)->interface_cached[fe->interface_no] < chunkno && chunkno <=upperbound )
				{
					cachesetflag = 1 ;
					(*iter_cache)->interface_cached[fe->interface_no] = chunkno ;
				}
				if( (*iter_cache)->interface_cached[fe->interface_no] >= chunkno )
				{
					cachesetflag = 1 ;
					(*iter_cache)->interface_cached[fe->interface_no] = chunkno ;
					(*iter_cache)->interface_state[fe->interface_no] = (int) (log(chunkno)/log((*iter_cache)->CMW_base)) ;
				}
				if( chunkno == (*iter_cache)->total_chunk )
				{
					(*iter_cache)->interface_state[fe->interface_no]++ ;
				}
				ForwardingEntry *tfe;
				bool foundoutput = false ;
				for (int i = 0; i < fwTable.size(); i++) {
					tfe = fwTable[i];
					andVector = (reverse_FID)&(*tfe->LID);
					if (andVector == (*tfe->LID)) {
						foundoutput = true ;
						break ;
					}
				}
				if(!foundoutput)
				{
					click_chatter("FW: can not find any out interface") ;
					return ;
				}
				//information update complete, now send back the data
				//note that the cache router will send all the info data under the scope
				for( HashTable<String, char*>::iterator iter_info = (*iter_cache)->chunk_info_cache[chunkID].begin() ;\
					iter_info != (*iter_cache)->chunk_info_cache[chunkID].end() ; iter_info++)
				{
					unsigned int packet_len ;
					unsigned char datatype = WAVE_PUBLISH_DATA ;
					unsigned int totalinfonum = 0 ;
					String fullID = fileID+chunkID+iter_info->first ;
					idlen = fullID.length()/PURSUIT_ID_LEN;
					packet_len = FID_LEN+1+sizeof(chunkno)+2+(idlen)*PURSUIT_ID_LEN+sizeof(totalinfonum)+\
								 (*iter_cache)->chunk_info_length[chunkID][iter_info->first] ;
					payload = Packet::make(20,NULL,packet_len,0) ;
					memcpy(payload->data(), reverse_FID._data, FID_LEN) ;
					memcpy(payload->data()+FID_LEN, &cachesetflag, 1) ;
					memcpy(payload->data()+1+FID_LEN, &chunkno, sizeof(chunkno)) ;
					memcpy(payload->data()+1+FID_LEN+sizeof(chunkno), &datatype, 1) ;
					memcpy(payload->data()+1+FID_LEN+sizeof(chunkno)+1, &idlen, 1) ;
					memcpy(payload->data()+1+FID_LEN+sizeof(chunkno)+2, fullID.c_str(), fullID.length()) ;
					memcpy(payload->data()+1+FID_LEN+sizeof(chunkno)+2+fullID.length(), &totalinfonum, sizeof(totalinfonum)) ;
					memcpy(payload->data()+1+FID_LEN+sizeof(chunkno)+2+fullID.length()+sizeof(totalinfonum),\
						   iter_info->second, (*iter_cache)->chunk_info_length[chunkID][iter_info->first]) ;

					newPacket = payload->push_mac_header(14);
					/*prepare the mac header*/
					/*destination MAC*/
					memcpy(newPacket->data(), tfe->dst->data(), MAC_LEN);
					/*source MAC*/
					memcpy(newPacket->data() + MAC_LEN, tfe->src->data(), MAC_LEN);
					memcpy(newPacket->data() + MAC_LEN + MAC_LEN, &data_type, 2);
					data_sent_byte += newPacket->length() ;
                    if( data_sent_byte >= oneGB ){
                        data_sent_byte = data_sent_byte - oneGB ;
                        data_sent_GB++ ;
                    }
					output(tfe->port).push(newPacket);
					return ;
				}
			}
		}
		if (!testFID.zero()) {
			for (out_links_it = out_links.begin(); out_links_it != out_links.end(); out_links_it++) {
				if ((counter == out_links.size()) && (pushLocally == false)) {
					payload = p->uniqueify();
				} else {
					payload = p->clone()->uniqueify();
				}
				data_forward_byte += payload->length() ;
				if(data_forward_byte >= oneGB)
				{
					data_forward_byte = data_forward_byte - oneGB ;
					data_forward_GB++ ;
				}
				fe = *out_links_it;
				if (gc->use_mac) {
					/*prepare the mac header*/
					/*destination MAC*/
					memcpy(payload->data(), fe->dst->data(), MAC_LEN);
					/*source MAC*/
					memcpy(payload->data() + MAC_LEN, fe->src->data(), MAC_LEN);
					data_sent_byte += payload->length() ;
					memcpy(payload->data()+15+FID_LEN, reverse_FID._data, FID_LEN) ;
                    if( data_sent_byte >= oneGB ){
                        data_sent_byte = data_sent_byte - oneGB ;
                        data_sent_GB++ ;
                    }
					/*push the packet to the appropriate ToDevice Element*/
					output(fe->port).push(payload);
				}
				counter++;
			}
		} else {
			/*all bits were 1 - probably from a link_broadcast strategy--do not forward*/
		}
		if ((out_links.size() == 0) && (!pushLocally)) {
			p->kill();
		}
	}else if(in_port == 4){
		WritablePacket* wp ;
		BABitvector fid2sub(FID_LEN*8) ;
		String fullID ;
		String chunkID ;
		String infoID ;
		String fileID ;
		unsigned int chunkno ;
		unsigned int totalchunknum ;
		unsigned int totalinfonum ;
		unsigned char cachesetflag ;
		unsigned char idlen ;
		unsigned char type ;
		ForwardingEntry *fe;
		type = *(p->data()) ;
		memcpy(fid2sub._data, p->data()+sizeof(unsigned char), FID_LEN) ;
		idlen = *(p->data()+sizeof(unsigned char)+FID_LEN) ;
		fullID = String( (const char*) (p->data()+sizeof(unsigned char)+FID_LEN+sizeof(idlen)), idlen*PURSUIT_ID_LEN) ;
		cachesetflag = *(p->data()+sizeof(unsigned char)+FID_LEN+sizeof(idlen)+idlen*PURSUIT_ID_LEN) ;
		memcpy(&chunkno, p->data()+sizeof(unsigned char)+FID_LEN+sizeof(idlen)+idlen*PURSUIT_ID_LEN+1, sizeof(chunkno)) ;
		memcpy(&totalchunknum, p->data()+sizeof(unsigned char)+FID_LEN+sizeof(idlen)+idlen*PURSUIT_ID_LEN+1+sizeof(chunkno), sizeof(totalchunknum)) ;
		memcpy(&totalinfonum, p->data()+sizeof(unsigned char)+FID_LEN+sizeof(idlen)+idlen*PURSUIT_ID_LEN+1+sizeof(chunkno)+sizeof(totalchunknum), sizeof(totalinfonum)) ;
		chunkID = fullID.substring(fullID.length()-2*PURSUIT_ID_LEN, PURSUIT_ID_LEN) ;
		infoID = fullID.substring(fullID.length()-PURSUIT_ID_LEN, PURSUIT_ID_LEN) ;
		fileID = fullID.substring(0, fullID.length()-2*PURSUIT_ID_LEN) ;

		BABitvector andVector(FID_LEN * 8);
		Vector<ForwardingEntry *>::iterator out_links_it;
		bool foundoutput = false ;
		/*Check all entries in my forwarding table and forward appropriately*/
		for (int i = 0; i < fwTable.size(); i++) {
			fe = fwTable[i];
			andVector = (fid2sub)&(*fe->LID);
			if (andVector == (*fe->LID)) {
				foundoutput = true ;
				break ;
			}
		}
		if(!foundoutput)
		{
			click_chatter("FW: can not find any out interface") ;
			return ;
		}
		for(Vector<CacheEntry*>::iterator iter_cache = cache.begin() ; iter_cache != cache.end() ; iter_cache++)
		{
			if( (*iter_cache)->match_file(fileID) )
			{
				(*iter_cache)->chunk_sent_data[chunkID].find_insert(StringSetItem(infoID));
				int upperbound = 0 ;
				for( int i = 0 ; i <= (*iter_cache)->interface_state[fe->interface_no] ; i++ )
				{
					upperbound += (int) pow((float) ((*iter_cache)->CMW_base), (float) i) ;
				}
				if( (*iter_cache)->interface_cached[fe->interface_no] < chunkno && chunkno <=upperbound )
				{
					cachesetflag = 1 ;
					if((*iter_cache)->chunk_sent_data[chunkID].size() == totalinfonum)
					{
						(*iter_cache)->interface_cached[fe->interface_no] = chunkno ;
						(*iter_cache)->chunk_sent_data[chunkID].clear() ;
					}
				}
				if( (*iter_cache)->interface_cached[fe->interface_no] >= chunkno )
				{
					cachesetflag = 1 ;
					if((*iter_cache)->chunk_sent_data[chunkID].size() == totalinfonum){
						(*iter_cache)->chunk_sent_data[chunkID].clear() ;
						(*iter_cache)->interface_cached[fe->interface_no] = chunkno ;
						(*iter_cache)->interface_state[fe->interface_no] = (int) (log(chunkno)/log((*iter_cache)->CMW_base)) ;
					}
				}
				if( chunkno == (*iter_cache)->total_chunk )
				{
					if((*iter_cache)->chunk_sent_data[chunkID].size() == totalinfonum){
						(*iter_cache)->chunk_sent_data[chunkID].clear() ;
						(*iter_cache)->interface_state[fe->interface_no]++ ;
					}
				}
				p->pull(sizeof(totalchunknum)) ;
				payload = p->uniqueify() ;
				memcpy(payload->data(), fid2sub._data, FID_LEN) ;
				memcpy(payload->data()+FID_LEN, &cachesetflag, 1) ;
				memcpy(payload->data()+FID_LEN+1, &chunkno, sizeof(chunkno)) ;
				memcpy(payload->data()+FID_LEN+1+sizeof(chunkno), &type, 1) ;
				memcpy(payload->data()+FID_LEN+2+sizeof(chunkno), &idlen, 1) ;
				memcpy(payload->data()+FID_LEN+3+sizeof(chunkno), fullID.c_str(), fullID.length()) ;
				newPacket = payload->push_mac_header(14);
				/*prepare the mac header*/
				/*destination MAC*/
				memcpy(newPacket->data(), fe->dst->data(), MAC_LEN);
				/*source MAC*/
				memcpy(newPacket->data() + MAC_LEN, fe->src->data(), MAC_LEN);
				memcpy(newPacket->data() + MAC_LEN + MAC_LEN, &data_type, 2);
				data_sent_byte += newPacket->length() ;
                if( data_sent_byte >= oneGB ){
                    data_sent_byte = data_sent_byte - oneGB ;
                    data_sent_GB++ ;
                }
				output(fe->port).push(newPacket);
				return ;
			}
		}
		CacheEntry* newcacheentry = new CacheEntry(fileID) ;
		for (int i = 0; i < fwTable.size(); i++) {
			newcacheentry->interface_state[fwTable[i]->interface_no] = 0 ;
			newcacheentry->interface_cached[fwTable[i]->interface_no] = 0 ;
		}
		newcacheentry->chunk_sent_data[chunkID].find_insert(StringSetItem(infoID));
		if(totalinfonum == 1)
		{
			newcacheentry->interface_cached[fe->interface_no] = chunkno ;
		}
		newcacheentry->total_chunk = totalchunknum ;
		cache.push_back(newcacheentry) ;
		p->pull(sizeof(totalchunknum)) ;
		payload = p->uniqueify() ;
		cachesetflag = 1 ;
		memcpy(payload->data(), fid2sub._data, FID_LEN) ;
		memcpy(payload->data()+FID_LEN, &cachesetflag, 1) ;
		memcpy(payload->data()+FID_LEN+1, &chunkno, sizeof(chunkno)) ;
		memcpy(payload->data()+FID_LEN+1+sizeof(chunkno), &type, 1) ;
		memcpy(payload->data()+FID_LEN+2+sizeof(chunkno), &idlen, 1) ;
		memcpy(payload->data()+FID_LEN+3+sizeof(chunkno), fullID.c_str(), fullID.length()) ;
		newPacket = payload->push_mac_header(14);
		/*prepare the mac header*/
		/*destination MAC*/
		memcpy(newPacket->data(), fe->dst->data(), MAC_LEN);
		/*source MAC*/
		memcpy(newPacket->data() + MAC_LEN, fe->src->data(), MAC_LEN);
		memcpy(newPacket->data() + MAC_LEN + MAC_LEN, &data_type, 2);
		data_sent_byte += newPacket->length() ;
        if( data_sent_byte >= oneGB ){
            data_sent_byte = data_sent_byte - oneGB ;
            data_sent_GB++ ;
        }
		output(fe->port).push(newPacket);
	} else if (in_port == 5) {
		unsigned char cachesetflag ;
		BABitvector fid2sub(FID_LEN*8) ;
		unsigned int chunkno ;
		unsigned int totalinfonum ;
		unsigned char idlen ;
		String fullID ;
		String fileID ;
		String chunkID ;
		String infoID ;

		memcpy(fid2sub._data, p->data()+14, FID_LEN) ;
		cachesetflag = *(p->data()+14+FID_LEN) ;
		BABitvector testFID(fid2sub);
		/*check if the packet must be pushed locally*/
		andVector = fid2sub & gc->iLID;
		if (andVector == gc->iLID) {
			p->pull(14 + FID_LEN+1);
			output(3).push(p);
			return ;
		}
		bool foundoutput = false ;
		testFID.negate();
		if (!testFID.zero()) {
			/*Check all entries in my forwarding table and forward appropriately*/
			for (int i = 0; i < fwTable.size(); i++) {
				fe = fwTable[i];
				andVector = (fid2sub)&(*fe->LID);
				if (andVector == (*fe->LID)) {
					EtherAddress src(p->data() + MAC_LEN);
					EtherAddress dst(p->data());
					if ((src.unparse().compare(fe->dst->unparse()) == 0) && (dst.unparse().compare(fe->src->unparse()) == 0)) {
						click_chatter("MAC: a loop from positive..I am not forwarding to the interface I received the packet from");
						continue;
					}
					foundoutput = true ;
					break ;
				}
			}
		} else {
			/*all bits were 1 - probably from a link_broadcast strategy--do not forward*/
		}
		if(!foundoutput)
		{
			click_chatter("can not find out interface") ;
			return ;
		}
		if(cachesetflag > 0)
		{
			bool cachefound = false ;
			memcpy(&chunkno, p->data()+14+FID_LEN+1, sizeof(chunkno)) ;
			idlen = *(p->data()+14+FID_LEN+1+sizeof(chunkno)+1) ;
			fullID = String( (const char*) (p->data()+14+FID_LEN+sizeof(chunkno)+3), idlen*PURSUIT_ID_LEN) ;
			fileID = fullID.substring(0, fullID.length()-2*PURSUIT_ID_LEN) ;
			chunkID = fullID.substring(fullID.length()-2*PURSUIT_ID_LEN, PURSUIT_ID_LEN) ;
			infoID = fullID.substring(fullID.length()-PURSUIT_ID_LEN, PURSUIT_ID_LEN) ;
			memcpy(&totalinfonum, p->data()+14+FID_LEN+sizeof(chunkno)+3+idlen*PURSUIT_ID_LEN, sizeof(totalinfonum)) ;
			cachesetflag-- ;
			unsigned int data_len = p->length()-14-FID_LEN-3-sizeof(chunkno)-fullID.length()-sizeof(totalinfonum) ;
			char* tempcache ;
			for(Vector<CacheEntry*>::iterator iter_cache = cache.begin() ; iter_cache != cache.end() ; iter_cache++)
			{
				if( (*iter_cache)->match_file(fileID) )
				{

					tempcache = (*iter_cache)->chunk_info_cache[chunkID].get(infoID) ;
					if( tempcache != (*iter_cache)->chunk_info_cache[chunkID].default_value() )
					{
						cachefound = true ;
						break ;
					}

					tempcache = (char*)malloc(data_len) ;
					memcpy(tempcache, p->data()+14+FID_LEN+3+sizeof(chunkno)+fullID.length()+sizeof(totalinfonum), data_len) ;
					(*iter_cache)->chunk_info_cache[chunkID][infoID] = tempcache ;
					current_cache_size += data_len ;
					(*iter_cache)->chunk_noofinfo[chunkID] = totalinfonum ;
					(*iter_cache)->chunk_info_length[chunkID][infoID] = data_len ;
					(*iter_cache)->total_len += data_len ;
					if(totalinfonum == (*iter_cache)->chunk_info_cache[chunkID].size())
					{
						(*iter_cache)->cached_chunks.push_back(chunkID) ;
						(*iter_cache)->total_chunk++ ;
					}
					if(current_cache_size > total_cache_size)
					{
						//cache overload, remove the least recently used one
						cache_replace++ ;
						if(cache_replace == Billion)
						{
							cache_replace = 0 ;
							cache_replace_Bill++ ;
						}
						CacheEntry* ce = cache[0] ;
						current_cache_size = current_cache_size - cache[0]->total_len ;
						cache.erase(cache.begin()) ;
						delete ce ;
					}

					break ;
				}
			}
			if(!cachefound)
			{
				CacheEntry* newcacheentry = new CacheEntry(fileID) ;
				newcacheentry->CMW_base = 2 ;
				newcacheentry->total_chunk = 0 ;
				for (int i = 0; i < fwTable.size(); i++) {
					newcacheentry->interface_state[fwTable[i]->interface_no] = 0 ;
					newcacheentry->interface_cached[fwTable[i]->interface_no] = 0 ;
				}
				newcacheentry->total_len = data_len ;
				tempcache = (char*)malloc(data_len) ;
				memcpy(tempcache, p->data()+14+FID_LEN+3+sizeof(chunkno)+fullID.length()+sizeof(totalinfonum), data_len) ;
				newcacheentry->chunk_info_cache[chunkID][infoID] = tempcache ;
				newcacheentry->chunk_info_length[chunkID][infoID] = data_len ;
				newcacheentry->chunk_noofinfo[chunkID] = totalinfonum ;
				newcacheentry->cached_chunks.clear() ;
				if( totalinfonum == 1 )
				{
					newcacheentry->cached_chunks.push_back(chunkID) ;
					newcacheentry->total_chunk = 1 ;
				}
				cache.push_back(newcacheentry) ;
			}
		}
		payload = p->uniqueify() ;
		memcpy(payload->data()+14+FID_LEN, &cachesetflag, sizeof(cachesetflag)) ;
		memcpy(payload->data(), fe->dst->data(), MAC_LEN);
		/*source MAC*/
		memcpy(payload->data() + MAC_LEN, fe->src->data(), MAC_LEN);
		data_sent_byte += payload->length() ;
        if( data_sent_byte >= oneGB ){
            data_sent_byte = data_sent_byte - oneGB ;
            data_sent_GB++ ;
        }
		/*push the packet to the appropriate ToDevice Element*/
		output(fe->port).push(payload);

	} else if (in_port == 1) {
        /**a packet has been pushed by the underlying network.**/
        /*check if it needs to be forwarded*/
        if (gc->use_mac) {
            memcpy(FID._data, p->data() + 14, FID_LEN);
        } else {
            memcpy(FID._data, p->data() + 28, FID_LEN);
        }
        BABitvector testFID(FID);

        testFID.negate();
        if (!testFID.zero()) {
            /*Check all entries in my forwarding table and forward appropriately*/
            for (int i = 0; i < fwTable.size(); i++) {
                fe = fwTable[i];
                andVector = (FID)&(*fe->LID);
                if (andVector == (*fe->LID)) {
					if (gc->use_mac) {
						EtherAddress src(p->data() + MAC_LEN);
						EtherAddress dst(p->data());
						if ((src.unparse().compare(fe->dst->unparse()) == 0) && (dst.unparse().compare(fe->src->unparse()) == 0)) {
							click_chatter("MAC: a loop from positive..I am not forwarding to the interface I received the packet from");
							continue;
						}
					} else {
						click_ip *ip = reinterpret_cast<click_ip *> ((unsigned char *)p->data());
						if ((ip->ip_src.s_addr == fe->dst_ip->in_addr().s_addr) && (ip->ip_dst.s_addr == fe->src_ip->in_addr().s_addr)) {
							click_chatter("IP: a loop from positive..I am not forwarding to the interface I received the packet from");
							continue;
						}
					}
					out_links.push_back(fe);

                }
            }
        } else {
            /*all bits were 1 - probably from a link_broadcast strategy--do not forward*/
        }
        /*check if the packet must be pushed locally*/
        andVector = FID & gc->iLID;
        if (andVector == gc->iLID) {
            pushLocally = true;
        }
        if (!testFID.zero()) {
            for (out_links_it = out_links.begin(); out_links_it != out_links.end(); out_links_it++) {
                if ((counter == out_links.size()) && (pushLocally == false)) {
                    payload = p->uniqueify();
                } else {
                    payload = p->clone()->uniqueify();
                }
				data_forward_byte += payload->length() ;
				if(data_forward_byte >= oneGB)
				{
					data_forward_byte = data_forward_byte - oneGB ;
					data_forward_GB++ ;
				}
                fe = *out_links_it;
                if (gc->use_mac) {
                    /*prepare the mac header*/
                    /*destination MAC*/
                    memcpy(payload->data(), fe->dst->data(), MAC_LEN);
                    /*source MAC*/
                    memcpy(payload->data() + MAC_LEN, fe->src->data(), MAC_LEN);
                    data_sent_byte += payload->length() ;
                    if( data_sent_byte >= oneGB ){
                        data_sent_byte = data_sent_byte - oneGB ;
                        data_sent_GB++ ;
                    }
                    /*push the packet to the appropriate ToDevice Element*/
                    output(fe->port).push(payload);
                } else {
                    click_ip *ip = reinterpret_cast<click_ip *> (payload->data());
                    ip->ip_src = fe->src_ip->in_addr();
                    ip->ip_dst = fe->dst_ip->in_addr();
                    ip->ip_tos = 0;
                    ip->ip_off = 0;
                    ip->ip_ttl = 250;
                    ip->ip_sum = 0;
                    ip->ip_sum = click_in_cksum((unsigned char *) ip, sizeof (click_ip));
#if NOTYET
                    click_udp *udp = reinterpret_cast<click_udp *> (ip + 1);
                    uint16_t len = udp->uh_ulen;
                    udp->uh_sum = 0;
                    unsigned csum = click_in_cksum((unsigned char *) udp, len);
                    udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
#endif
                    output(fe->port).push(payload);
                }
                counter++;
            }
        } else {
            /*all bits were 1 - probably from a link_broadcast strategy--do not forward*/
        }
        if (pushLocally) {
            if (gc->use_mac) {
                p->pull(14 + FID_LEN);
                output(0).push(p);
            } else {
                p->pull(20 + 8 + FID_LEN);
                output(0).push(p);
            }
        }

        if ((out_links.size() == 0) && (!pushLocally)) {
            p->kill();
        }
    }
}

bool CacheEntry::match_file(String& _fileID)
{
	if(_fileID == fileID)
		return true ;
	else
		return false ;
}
bool CacheEntry::match_filechunk(String& _fileID, String& _chunkID)
{
	if( _fileID == fileID )
	{
		for( int i = 0 ; i < cached_chunks.size() ; i++)
		{
			if(_chunkID == cached_chunks[i])
				return true ;
		}
	}
	return false ;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Forwarder)
ELEMENT_PROVIDES(ForwardingEntry)
ELEMENT_PROVIDES(CacheEntry)
