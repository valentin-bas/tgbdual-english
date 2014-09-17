/*--------------------------------------------------
   TGB Dual - Gameboy Emulator -
   Copyright (C) 2004  Hii

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// �l�b�g���[�N�ΐ�p
#pragma once

#include "./lib/socket_lib.h"
#include "./lib/thread.h"
#include "./lib/critical_section.h"

#include <queue>
#include <string>

using namespace std;

template <class T>
class netplay : public thread{
public:
	typedef T data_type;
	// �T�[�o�[�p�̃R���X�g���N�^
	netplay(int port){
		server=true;
		host="";
		this->port=port;
		my_sram=opp_sram=NULL;
		alive=true;
		prepared=false;
		start();
	}
	// �N���C�A���g�p�̃R���X�g���N�^
	netplay(string &host,int port){
		server=false;
		this->host=host;
		this->port=port;
		my_sram=opp_sram=NULL;
		alive=true;
		prepared=false;
		start();
	}

	~netplay(){
		char *ms=(char*)my_sram,*os=(char*)opp_sram;
		if (my_sram) delete []ms;
		if (opp_sram) delete []os;

		// �X���b�h�I���܂őҋ@
		if (running()){
			if (done_prepare()){
				alive=false;
				join();
			}
			else{
				// �ʖڂł��ȁc���̂܂܏I���΃X���b�h��Terminate���Ă����͂�
			}
		}
	}

	// �T�[�o�[�H
	bool is_server(){ return server; }

	// �����������Ă���H
	bool done_prepare(){ return prepared; }

	// �l�b�g���[�N�x�� �����H
	int network_delay(){ return delay; }

	// �ڑ������H
	bool connected(){ return s!=NULL; }

	// SRAM�̑���M
	void send_sram(char *data,int size){
		my_sram_size=size;
		char *tmp=new char[size];
		memcpy(tmp,data,size);
		my_sram=tmp;
	}
	pair<char*,int> get_sram(){
		return pair<char*,int>((char*)opp_sram,(int)opp_sram_size);
	}

	// �L�[�f�[�^�̑���M
	void send_keydata(data_type &dat){
		try{
			critical_lock cl(cs);
			my_keydata.push(dat);
			send_packet(packet("KEY ",sizeof(data_type),(char*)&dat));
		} catch(socket_exception&){}
	}
	int get_keydata_num(){
		critical_lock cl(cs);
		return min(my_keydata.size(),opp_keydata.size());
	}
	pair<data_type,data_type> pop_keydata(){
		critical_lock cl(cs);
		data_type f=my_keydata.front(),s=opp_keydata.front();
		my_keydata.pop();
		opp_keydata.pop();
		return pair<data_type,data_type>(f,s);
	}

	// �`���b�g�f�[�^�̑���M
	void send_message(string &msg){
		try{
			critical_lock cl(cs);
			send_packet(packet("MSG ",msg.length()+1,(char*)msg.c_str()));
		}catch(socket_exception&){}
	}
	int get_message_num(){ return message.size(); }
	string get_message(){
		if (message.size()>0){
			string ret=message.front();
			message.pop();
			return ret;
		}
		else return string("");
	}

	// ��������
	void run(){
		// �܂��͐ڑ�
		if (server){
			server_socket *ss=new server_socket(port);
			s=ss->accept();
			delete ss; // �T�[�o�[�\�P�b�g���ĕ��Ă����v�H
		}
		else{
			s=new socket_obj(host,port);
		}

		s->set_no_delay(true);

		// SRAM�]���v�������Ă��瑗��M
		while(my_sram==NULL) thread::sleep(10);
		send_packet(packet("SRAM",my_sram_size,(char*)my_sram));

		packet p=recv_packet();
		if (p.tag!="SRAM"){ // �ǂȂ�����c
			delete s;
			s=NULL;
			return;
		}
		opp_sram=p.dat;
		opp_sram_size=p.size;

		// �x�����x���v�����Ă��� 20�񂮂炢�̃��f�B�A���ł�����
		{
			data_type dmy;
			if (server){
				int d[20];
				for (int i=0;i<20;i++){
					DWORD start=timeGetTime();
					send_packet(packet("TEST",sizeof(dmy),(char*)&dmy));
					packet p=recv_packet();
					d[i]=timeGetTime()-start;
					delete []p.dat;
				}
				sort(d,d+19);
				delay=d[9];
				send_packet(packet("DELY",4,(char*)&delay));
			}
			else{
				for (int i=0;i<20;i++){
					packet p=recv_packet();
					send_packet(packet("TEST",sizeof(dmy),(char*)&dmy));
					delete []p.dat;
				}
				packet p=recv_packet();
				delay=*(int*)p.dat;
				delete []p.dat;
			}
		}

		prepared=true;

		// �p�P�b�g�������[�v
		while (alive){
			if (!s->connected())
				break;
			else if (s->ready_to_receive()){
				packet p;
				try{
					p=recv_packet();
				} catch(socket_exception&){
					break;
				}

				if (p.tag=="KEY "){
					if (p.size==sizeof(data_type))
						push_opp_key(*(data_type*)p.dat);
				}
				else if (p.tag=="MSG "){
					if (p.size==strlen(p.dat)+1) // �ЂƂ܂��ȒP�ȃ`�F�b�N��
						message.push(string(p.dat));
				}
				delete []p.dat;
			}
			else
				sleep(4); // �K�x�ɑ҂�
		}

		delete s;
		s=NULL;
	}

private:
	// �p�P�b�g��������
	class packet{
	public:
		packet(char *tag="",int size=0,char *dat=NULL){
			this->tag=string(tag);
			this->size=size;
			this->dat=dat;
		}
		string tag;
		int size;
		char *dat;
	};
	void send_packet(packet p){
		if (s==NULL) return; // �Ƃ肠������������Ƃ������ƂŁB
		if (p.tag.length()!=4) return; // �����
		try{
			s->send((void*)p.tag.c_str(),4);
			s->send(&p.size,4);
			s->send(p.dat,p.size);
		}catch(socket_exception&){ /* �������� */ }
	}
	packet recv_packet(){
		if (s==NULL) return packet();

		char *tag=new char[5];
		s->read(tag,4);
		tag[4]='\0';

		int size;
		s->read(&size,4);

		char *dat=new char[size];
		s->read(dat,size);
		packet p=packet(tag,size,dat);
		delete []tag;
		return p;
	}

	// SRAM����
	volatile char *my_sram,*opp_sram;
	volatile int my_sram_size,opp_sram_size;

	// �L�[�f�[�^�̊i�[
	queue<data_type> my_keydata,opp_keydata;
	void push_opp_key(data_type &dat){
		critical_lock cl(cs);
		opp_keydata.push(dat);
	}

	// �`���b�g�f�[�^�i�[
	queue<string> message;

	// �ڑ�����
	bool server;
	string host;
	int port;
	socket_obj *s;

	// �f�B���C (in ms)
	volatile int delay;

	volatile bool prepared;
	bool alive;
	critical_section cs;
};
