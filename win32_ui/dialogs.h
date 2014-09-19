/*--------------------------------------------------
   TGB Dual - Gameboy Emulator -
   Copyright (C) 2001  Hii

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

#include "keymap.h"

static int rom_size_tbl[]={2,4,8,16,32,64,128,256,512};

static char tmp_sram_name[2][256];
static const char mbc_types[0x101][40]={"ROM Only","ROM + MBC1","ROM + MBC1 + RAM","ROM + MBC1 + RAM + Battery","Unknown","ROM + MBC2","ROM + MBC2 + Battery","Unknown",
									"ROM + RAM" ,"ROM + RAM + Battery","Unknown","ROM + MMM01","ROM + MMM01 + SRAM","ROM + MMM01 + Battery","Unknown",
									"ROM + MBC3 + TIMER + Battery","ROM + MBC3 + TIMER + RAM + Battery","ROM + MBC3","ROM + MBC3 + RAM","ROM + MBC3 + RAM + Battery",
									"Unknown","Unknown","Unknown","Unknown","Unknown",
									"ROM + MBC5","ROM + MBC5 + RAM","ROM + MBC5 + RAM + Battery","ROM + MBC5 + RUMBLE","ROM + MBC5 + RUMBLE + SRAM","ROM + MBC5 + RUMBLE + SRAM + Battery",
									"Pocket Camera","","","MBC7? + EEPROM + MOTIONSENSOR",//#22
									"","","","","","","","","","","","","",//#2F
									"","","","","","","","","","","","","","","","",//#3F
									"","","","","","","","","","","","","","","","",//#4F
									"","","","","","","","","","","","","","","","",//#5F
									"","","","","","","","","","","","","","","","",//#6F
									"","","","","","","","","","","","","","","","",//#7F
									"","","","","","","","","","","","","","","","",//#8F
									"","","","","","","","","","","","","","","","",//#9F
									"","","","","","","","","","","","","","","","",//#AF
									"","","","","","","","","","","","","","","","",//#BF
									"","","","","","","","","","","","","","","","",//#CF
									"","","","","","","","","","","","","","","","",//#DF
									"","","","","","","","","","","","","","","","",//#EF
									"","","","","","","","","","","","","","Bandai TAMA5","Hudson HuC-3","Hudson HuC-1",//#FF
									"mmm01" // ����
};
static byte org_gbtype[2];
static bool sys_win2000;

void save_sram(BYTE *buf,int size,int num)
{
	if (strstr(tmp_sram_name[num],".srt"))
		return;

	int sram_tbl[]={1,1,1,4,16,8};
	char cur_di[256],sv_dir[256];
	GetCurrentDirectory(256,cur_di);
	config->get_save_dir(sv_dir);
	SetCurrentDirectory(sv_dir);
	FILE *fs=fopen(tmp_sram_name[num],"wb");
	fwrite(buf,1,0x2000*sram_tbl[size],fs);
	if ((g_gb[num]->get_rom()->get_info()->cart_type>=0x0f)&&(g_gb[num]->get_rom()->get_info()->cart_type<=0x13)){
		int tmp=render[0]->get_timer_state();
		fwrite(&tmp,4,1,fs);
	}
	fclose(fs);
	SetCurrentDirectory(cur_di);
}

void load_key_config(int num)
{
	int buf[16];
	key_dat keys[8]; // a,b,select,start,down,up,left,right �̏�

	config->get_key_setting(buf,num);

	keys[0].device_type=buf[0];
	keys[1].device_type=buf[2];
	keys[2].device_type=buf[4];
	keys[3].device_type=buf[6];
	keys[4].device_type=buf[8];
	keys[5].device_type=buf[10];
	keys[6].device_type=buf[12];
	keys[7].device_type=buf[14];
	keys[0].key_code=buf[1];
	keys[1].key_code=buf[3];
	keys[2].key_code=buf[5];
	keys[3].key_code=buf[7];
	keys[4].key_code=buf[9];
	keys[5].key_code=buf[11];
	keys[6].key_code=buf[13];
	keys[7].key_code=buf[15];

	render[num]->set_key(keys);

	keys[0].device_type=config->koro_key[0];
	keys[1].device_type=config->koro_key[2];
	keys[2].device_type=config->koro_key[4];
	keys[3].device_type=config->koro_key[6];
	keys[0].key_code=config->koro_key[1];
	keys[1].key_code=config->koro_key[3];
	keys[2].key_code=config->koro_key[5];
	keys[3].key_code=config->koro_key[7];

	render[num]->set_koro_key(keys);

	render[num]->set_koro_analog(config->koro_use_analog);
	render[num]->set_koro_sensitivity(config->koro_sensitive);

	render[num]->set_use_ffb(config->use_ffb);

	col_filter cof;
	cof.r_def=config->r_def;
	cof.g_def=config->g_def;
	cof.b_def=config->b_def;
	cof.r_div=config->r_div;
	cof.g_div=config->g_div;
	cof.b_div=config->b_div;
	cof.r_r=config->r_r;
	cof.r_g=config->r_g;
	cof.r_b=config->r_b;
	cof.g_r=config->g_r;
	cof.g_g=config->g_g;
	cof.g_b=config->g_b;
	cof.b_r=config->b_r;
	cof.b_g=config->b_g;
	cof.b_b=config->b_b;
	render[num]->set_filter(&cof);
}

typedef int (WINAPI *unarc)(const HWND _hwnd, LPCSTR _szCmdLine,LPSTR _szOutput, const DWORD _dwSize);

BYTE *load_archive(char *path, int *size)
{
	char log[256];
	char cmd_line[128];
	char tmp_dir[256];

	HMODULE hModule=NULL;
	WIN32_FIND_DATA wfd;
	HANDLE hFind;
	char ext_filename[256];

	BYTE *ret;
	unarc arc;

	GetTempPath(256,tmp_dir);

	if (strstr(path,".lzh")){ // LZH
		hModule=LoadLibrary("UnLha32.dll");
		if (!hModule){
			MessageBox(hWnd,"UnLha32.dll�����݂��܂���BLZH���ɂ��𓀂ł��܂���B","TGB Dual",MB_OK);
			return NULL;
		}
		arc=(unarc)GetProcAddress(hModule,"Unlha");
		sprintf(cmd_line,"e \"%s\" \"%s\" *.gb \"%s\" *.gbc",path,tmp_dir,tmp_dir);
		arc(hWnd,cmd_line,log,256);

		sprintf(ext_filename,"%s*.gb",tmp_dir);

		if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
			sprintf(ext_filename,"%s*.gbc",tmp_dir,tmp_dir);
			if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
				MessageBox(hWnd,"LZH �t�@�C���� GB �t�@�C�����܂�ł��܂���","TGB Dual",MB_OK);
				FreeLibrary(hModule);
				return NULL;
			}
		}
		sprintf(ext_filename,"%s%s",tmp_dir,wfd.cFileName);
		FindClose(hFind);
	}
	else if (strstr(path,".zip")){ // ZIP
		hModule=LoadLibrary("UnZip32.dll");
		if (!hModule){
			MessageBox(hWnd,"UnZip32.dll�����݂��܂���BZIP���ɂ��𓀂ł��܂���B","TGB Dual",MB_OK);
			return NULL;
		}
		arc=(unarc)GetProcAddress(hModule,"UnZip");
		sprintf(cmd_line,"-x -o -j \"%s\" \"%s\" *.gb",path,tmp_dir);
		arc(hWnd,cmd_line,log,512);

		sprintf(ext_filename,"%s*.gb",tmp_dir,tmp_dir);

		if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
			sprintf(cmd_line,"-x -o -j \"%s\" \"%s\" *.gbc",path,tmp_dir);
			arc(hWnd,cmd_line,log,512);

			sprintf(ext_filename,"%s*.gbc",tmp_dir,tmp_dir);
			if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
				MessageBox(hWnd,"ZIP �t�@�C���� GB �t�@�C�����܂�ł��܂���","TGB Dual",MB_OK);
				FreeLibrary(hModule);
				return NULL;
			}
		}
		sprintf(ext_filename,"%s%s",tmp_dir,wfd.cFileName);
		FindClose(hFind);
	}
	else if (strstr(path,".rar")){//RAR����
		hModule=LoadLibrary("UnRAR32.dll");
		if (!hModule){
			MessageBox(hWnd,"UnRAR32.dll�����݂��܂���BRAR���ɂ��𓀂ł��܂���B","TGB Dual",MB_OK);
			return NULL;
		}
		arc=(unarc)GetProcAddress(hModule,"Unrar");
		sprintf(cmd_line,"-x -o \"%s\" \"%s\" *.gb",path,tmp_dir);
		arc(hWnd,cmd_line,log,512);

		sprintf(ext_filename,"%s*.gb",tmp_dir,tmp_dir);

		if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
			sprintf(cmd_line,"-x -o \"%s\" \"%s\" *.gbc",path,tmp_dir);
			arc(hWnd,cmd_line,log,512);

			sprintf(ext_filename,"%s*.gbc",tmp_dir,tmp_dir);
			if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
				MessageBox(hWnd,"RAR �t�@�C���� GB �t�@�C�����܂�ł��܂���","TGB Dual",MB_OK);
				FreeLibrary(hModule);
				return NULL;
			}
		}
		sprintf(ext_filename,"%s%s",tmp_dir,wfd.cFileName);
		FindClose(hFind);
	}
	else if (strstr(path,".cab")){//CAB����
		hModule=LoadLibrary("cab32.dll");
		if (!hModule){
			MessageBox(hWnd,"cab32.dll�����݂��܂���Bcab���ɂ��𓀂ł��܂���B","TGB Dual",MB_OK);
			return NULL;
		}
		arc=(unarc)GetProcAddress(hModule,"Cab");
		sprintf(cmd_line,"-x -o \"%s\" \"%s\" *.gb",path,tmp_dir);
		arc(hWnd,cmd_line,log,512);

		sprintf(ext_filename,"%s*.gb",tmp_dir,tmp_dir);

		if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
			sprintf(cmd_line,"-x -o \"%s\" \"%s\" *.gbc",path,tmp_dir);
			arc(hWnd,cmd_line,log,512);

			sprintf(ext_filename,"%s*.gbc",tmp_dir,tmp_dir);
			if ((hFind=FindFirstFile(ext_filename,&wfd))==INVALID_HANDLE_VALUE){
				MessageBox(hWnd,"CAB �t�@�C���� GB �t�@�C�����܂�ł��܂���","TGB Dual",MB_OK);
				FreeLibrary(hModule);
				return NULL;
			}
		}
		sprintf(ext_filename,"%s%s",tmp_dir,wfd.cFileName);
		FindClose(hFind);
	}
	else{
		//MessageBox(hWnd,"���̃t�@�C�������s���邱�Ƃ͂ł��܂���","TGB Dual",MB_OK);
		return NULL;
	}

	FILE *file;
	file=fopen(ext_filename,"rb");
	fseek(file,0,SEEK_END);
	*size=ftell(file);
	fseek(file,0,SEEK_SET);
	ret=(BYTE*)malloc(*size);
	fread(ret,1,*size,file);
	fclose(file);

	sprintf(ext_filename,"%s*.gb",tmp_dir);

	hFind=FindFirstFile(ext_filename,&wfd);
	do{
		sprintf(ext_filename,"%s%s",tmp_dir,wfd.cFileName);
		SetFileAttributes(ext_filename,FILE_ATTRIBUTE_NORMAL);
		DeleteFile(ext_filename);
	}
	while(FindNextFile(hFind,&wfd));
	FindClose(hFind);
	sprintf(ext_filename,"%s*.gbc",tmp_dir);

	hFind=FindFirstFile(ext_filename,&wfd);
	do{
		sprintf(ext_filename,"%s%s",tmp_dir,wfd.cFileName);
		SetFileAttributes(ext_filename,FILE_ATTRIBUTE_NORMAL);
		DeleteFile(ext_filename);
	}
	while(FindNextFile(hFind,&wfd));
	FindClose(hFind);

	if (hModule)
		FreeLibrary(hModule);

	return ret;
}

HMODULE h_gbr_dll;

bool load_rom(char *buf,int num)
{
	FILE *file;
	int size;
	BYTE *dat;
	char *p=buf;

	p=strrchr(buf,'.');
	if (p)
		while(*p!='\0') *(p++)=tolower(*p);

	if (strstr(buf,".gbr")){
		file=fopen(buf,"rb");
		if (!file) return false;

		fseek(file,0,SEEK_END);
		size=ftell(file);
		fseek(file,0,SEEK_SET);
		dat=(BYTE*)malloc(size);
		fread(dat,1,size,file);
		fclose(file);

		if (g_gb[0]){
			if (g_gb[0]->get_rom()->has_battery())
				save_sram(g_gb[0]->get_rom()->get_sram(),g_gb[0]->get_rom()->get_info()->ram_size,0);
			delete g_gb[0];
			g_gb[0]=NULL;
		}
		if (g_gb[1]){
			if (g_gb[1]->get_rom()->has_battery())
				save_sram(g_gb[1]->get_rom()->get_sram(),g_gb[1]->get_rom()->get_info()->ram_size,1);
			delete g_gb[1];
			g_gb[1]=NULL;
			delete render[1];
			render[1]=NULL;
		}
		if (g_gbr){
			FreeLibrary(h_gbr_dll);
			delete g_gbr;
			g_gbr=NULL;
		}

		char cur_di[256],dll_dir[256],tmp[256];
		GetCurrentDirectory(256,cur_di);
		config->get_dev_dir(dll_dir);
		SetCurrentDirectory(dll_dir);

		gbr_procs *procs;
		h_gbr_dll=LoadLibrary("tgbr_dll.dll");
		if (h_gbr_dll){
			procs=((gbr_procs*(*)())GetProcAddress(h_gbr_dll,"get_interface"))();
			g_gbr=new gbr(render[0],procs);
			g_gbr->load_rom(dat,size);

			if (config->sound_enable[4]){
				g_gbr->set_enable(0,config->sound_enable[0]?true:false);
				g_gbr->set_enable(1,config->sound_enable[1]?true:false);
				g_gbr->set_enable(2,config->sound_enable[2]?true:false);
				g_gbr->set_enable(3,config->sound_enable[3]?true:false);
			}
			else{
				g_gbr->set_enable(0,false);
				g_gbr->set_enable(1,false);
				g_gbr->set_enable(2,false);
				g_gbr->set_enable(3,false);
			}
			g_gbr->set_effect(1,config->b_echo);
			g_gbr->set_effect(0,config->b_lowpass);

			strcpy(tmp_sram_name[0],buf);

			SetWindowText(hWnd,buf);
			sprintf(tmp,"Load GBR \"%s\" \n\n",buf);
			SendMessage(hWnd,WM_OUTLOG,0,(LPARAM)tmp);
		}
		else{
			MessageBox(hWnd,"tgbr_dll.dll�����݂��܂���B���̃t�@�C���͎��s�ł��܂���B","TGB Dual Notice",MB_OK);
		}
		SetCurrentDirectory(cur_di);

		return true;
	}
	else if (strstr(buf,".gb")||strstr(buf,".gbc")){
		file=fopen(buf,"rb");
		if (!file) return false;
		fseek(file,0,SEEK_END);
		size=ftell(file);
		fseek(file,0,SEEK_SET);
		dat=(BYTE*)malloc(size);
		fread(dat,1,size,file);
		fclose(file);
	}
	else
		if (!(dat=load_archive(buf,&size)))
			return false;

	if ((num==1)&&(!render[1])){
		render[1]=new dx_renderer(hWnd_sub,hInstance);
		render[1]->set_render_pass(config->render_pass);
		load_key_config(1);
	}
	if (!g_gb[num]){
		g_gb[num]=new gb(render[num],true,(num)?false:true);
		g_gb[num]->set_target(NULL);

		if (g_gb[num?0:1]){
			g_gb[0]->set_target(g_gb[1]);
			g_gb[1]->set_target(g_gb[0]);
		}

		if (config->sound_enable[4]){
			g_gb[num]->get_apu()->get_renderer()->set_enable(0,config->sound_enable[0]?true:false);
			g_gb[num]->get_apu()->get_renderer()->set_enable(1,config->sound_enable[1]?true:false);
			g_gb[num]->get_apu()->get_renderer()->set_enable(2,config->sound_enable[2]?true:false);
			g_gb[num]->get_apu()->get_renderer()->set_enable(3,config->sound_enable[3]?true:false);
		}
		else{
			g_gb[num]->get_apu()->get_renderer()->set_enable(0,false);
			g_gb[num]->get_apu()->get_renderer()->set_enable(1,false);
			g_gb[num]->get_apu()->get_renderer()->set_enable(2,false);
			g_gb[num]->get_apu()->get_renderer()->set_enable(3,false);
		}
		g_gb[num]->get_apu()->get_renderer()->set_echo(config->b_echo);
		g_gb[num]->get_apu()->get_renderer()->set_lowpass(config->b_lowpass);
	}
	else{
		if (g_gb[num]->get_rom()->has_battery())
			save_sram(g_gb[num]->get_rom()->get_sram(),g_gb[num]->get_rom()->get_info()->ram_size,num);
	}
	if (g_gbr){
		FreeLibrary(h_gbr_dll);
		delete g_gbr;
		g_gbr=NULL;
	}

	int tbl_ram[]={1,1,1,4,16,8};//0��1�͕ی�
	char sram_name[256],cur_di[256],sv_dir[256];
	BYTE *ram;
	int ram_size=0x2000*tbl_ram[dat[0x149]];
	char *suffix=num?".sa2":".sav";
	{
		char *p=(char*)_mbsrchr((unsigned char*)buf,(unsigned char)'\\');
		if (!p) p=buf; else p++;
		strcpy(sram_name,p);
		p=(char*)_mbsrchr((unsigned char*)sram_name,(unsigned char)'.');
		if (p) strcpy(p,suffix);
		else strcat(sram_name,suffix);
	}
	GetCurrentDirectory(256,cur_di);
	config->get_save_dir(sv_dir);
	SetCurrentDirectory(sv_dir);

	{
		char tmp_sram[256];
		strcpy(tmp_sram,sram_name);

		// ���̂܂܁A�擪�̃s���I�h����g���q�ɁA����ɂ�����g���qRAM���A��3�ʂ肵��ׂȂ�����c
		int i;
		for (i=0;i<3;i++){
			if (i==1) strcpy(strstr(tmp_sram,"."),suffix);
			else if (i==2) strcpy(strstr(tmp_sram,"."),num?".ra2":"ram");

			FILE *fs=fopen(tmp_sram,"rb");
			if (fs){
				ram=(BYTE*)malloc(ram_size);
				fread(ram,1,ram_size,fs);
				fseek(fs,0,SEEK_END);
				if (ftell(fs)&0xff){
					int tmp;
					fseek(fs,-4,SEEK_END);
					fread(&tmp,4,1,fs);
					if (render[num])
						render[num]->set_timer_state(tmp);
				}
				fclose(fs);
				break;
			}
		}
		if (i==3){
			ram=(BYTE*)malloc(ram_size);
			memset(ram,0,ram_size);
		}
	}
	strcpy(tmp_sram_name[num],sram_name);

	SetCurrentDirectory(cur_di);

	org_gbtype[num]=dat[0x143]&0x80;

	if (config->gb_type==1)
		dat[0x143]&=0x7f;
	else if (config->gb_type>=3)
		dat[0x143]|=0x80;

	g_gb[num]->set_use_gba(config->gb_type==0?config->use_gba:(config->gb_type==4?true:false));
	g_gb[num]->load_rom(dat,size,ram,ram_size);

	free(dat);
	free(ram);

	char pb[256];
	sprintf(pb,"Load ROM \"%s\" slot[%d] :\ntype-%d:%s\nsize=%dKB : name=%s\n\n",buf,num+1,g_gb[num]->get_rom()->get_info()->cart_type,mbc_types[g_gb[num]->get_rom()->get_info()->cart_type],size/1024,g_gb[num]->get_rom()->get_info()->cart_name);
	SendMessage(hWnd,WM_OUTLOG,0,(LPARAM)pb);


	if (num==0)
		SetWindowText(hWnd,g_gb[num]->get_rom()->get_info()->cart_name);
	else
		SetWindowText(hWnd_sub,g_gb[num]->get_rom()->get_info()->cart_name);

	return true;
}

bool load_rom_only(char *buf,int num)
{
	FILE *file;
	int size;
	BYTE *dat;
	char *p=buf;

	p=strrchr(buf,'.');
	if (p)
		while(*p!='\0') *(p++)=tolower(*p);

	if (strstr(buf,".gb")||strstr(buf,".gbc")){
		file=fopen(buf,"rb");
		if (!file) return false;
		fseek(file,0,SEEK_END);
		size=ftell(file);
		fseek(file,0,SEEK_SET);
		dat=(BYTE*)malloc(size);
		fread(dat,1,size,file);
		fclose(file);
	}
	else
		if (!(dat=load_archive(buf,&size)))
			return false;

	int tbl_ram[]={1,1,1,4,16,8};//0��1�͕ی�
	char sram_name[256],cur_di[256],sv_dir[256];
	BYTE *ram;
	int ram_size=0x2000*tbl_ram[dat[0x149]];
	char *suffix=num?".sa2":".sav";
	{
		char *p=(char*)_mbsrchr((unsigned char*)buf,(unsigned char)'\\');
		if (!p) p=buf; else p++;
		strcpy(sram_name,p);
		p=(char*)_mbsrchr((unsigned char*)sram_name,(unsigned char)'.');
		if (p) strcpy(p,suffix);
		else strcat(sram_name,suffix);
	}
	GetCurrentDirectory(256,cur_di);
	config->get_save_dir(sv_dir);
	SetCurrentDirectory(sv_dir);

	{
		char tmp_sram[256];
		strcpy(tmp_sram,sram_name);

		// ���̂܂܁A�擪�̃s���I�h����g���q�ɁA����ɂ�����g���qRAM���A��3�ʂ肵��ׂȂ�����c
		int i;
		for (i=0;i<3;i++){
			if (i==1) strcpy(strstr(tmp_sram,"."),suffix);
			else if (i==2) strcpy(strstr(tmp_sram,"."),num?".ra2":"ram");

			FILE *fs=fopen(tmp_sram,"rb");
			if (fs){
				ram=(BYTE*)malloc(ram_size);
				fread(ram,1,ram_size,fs);
				fseek(fs,0,SEEK_END);
				if (ftell(fs)&0xff){
					int tmp;
					fseek(fs,-4,SEEK_END);
					fread(&tmp,4,1,fs);
					if (render[num])
						render[num]->set_timer_state(tmp);
				}
				fclose(fs);
				break;
			}
		}
		if (i==3){
			ram=(BYTE*)malloc(ram_size);
			memset(ram,0,ram_size);
		}
	}
	strcpy(tmp_sram_name[num],sram_name);

	SetCurrentDirectory(cur_di);

	org_gbtype[num]=dat[0x143]&0x80;

	if (config->gb_type==1)
		dat[0x143]&=0x7f;
	else if (config->gb_type>=3)
		dat[0x143]|=0x80;

	g_gb[num]->set_use_gba(config->gb_type==0?config->use_gba:(config->gb_type==4?true:false));
	g_gb[num]->load_rom(dat,size,ram,ram_size);
	free(dat);
	free(ram);

	char pb[256];

	if (num==0){
		sprintf(pb,"Load ROM \"%s\" slot[%d] :\ntype-%d:%s\nsize=%dKB : name=%s\n\n",buf,num+1,g_gb[num]->get_rom()->get_info()->cart_type,mbc_types[g_gb[num]->get_rom()->get_info()->cart_type],
			rom_size_tbl[g_gb[num]->get_rom()->get_info()->rom_size]*16,g_gb[num]->get_rom()->get_info()->cart_name);
		SendMessage(hWnd,WM_OUTLOG,0,(LPARAM)pb);
		SetWindowText(hWnd,g_gb[num]->get_rom()->get_info()->cart_name);
	}

	return true;
}

void free_rom(int slot)
{
	if (render[slot]){
		render[slot]->set_sound_renderer(NULL);
		// ���Ȃ��Ă��鉹�������B������悭�Ȃ��Ȃ��B
		render[slot]->pause_sound();
		render[slot]->resume_sound();
	}

	if (g_gb[slot]){
		if (g_gb[slot]->get_rom()->has_battery())
			save_sram(g_gb[slot]->get_rom()->get_sram(),g_gb[slot]->get_rom()->get_info()->ram_size,slot);
		delete g_gb[slot];
		g_gb[slot]=NULL;
	}
	if (slot==0){
		if (g_gbr){
			FreeLibrary(h_gbr_dll);
			delete g_gbr;
			g_gbr=NULL;
		}
		BYTE *buf;
		buf=(BYTE*)calloc(160*144,2);
		render[slot]->show_fps(false);
		render[slot]->render_screen(buf,160,144,16);
		render[slot]->show_fps(config->show_fps);
		free(buf);

		SetWindowText(hWnd,WINDOW_TITLE_UNLOADED);
	}
	else{
		if (render[slot]){
			delete render[slot];
			render[slot]=NULL;
		}
		if (dmy_render){
			delete dmy_render;
			dmy_render=NULL;
		}
		if (g_gb[0]) g_gb[0]->set_target(NULL);
	}
	tmp_sram_name[slot][0]='\0';
}

//-----------------------------------

static void elapse_time(int fps)
{
	static DWORD lastdraw=0,rest=0;
	int elapse_wait=(1000<<16)/fps;

	DWORD t=timeGetTime();

	rest=(rest&0xffff)+elapse_wait;

	DWORD wait=rest>>16; 
	DWORD elp=(DWORD)(t-lastdraw);

	if (elp>=wait){
		lastdraw=t;
		return;
	}

	if (wait-elp>=4)
		Sleep(wait-elp-3);
	while ((timeGetTime()-lastdraw)<wait);

	lastdraw += wait;
}

static struct GBDEV_DAT{
	void (*trash_device)();
	byte (*send_dat)(byte);
	bool (*get_led)();
} dev_callback;

static struct gbdev_dll{
	char file_name[256];
	char device_name[256];
} dll_dat[256];

static int dev_count=0;
static HMODULE hDevDll=NULL;
static bool dev_loaded;

static void trush_device()
{
	void (*uninit)();
	g_gb[0]->unhook_extport();
	uninit=(void(*)())GetProcAddress(hDevDll,"UninitDevice");
	uninit();
	dev_loaded=false;
}

static byte send_dat(byte dat)
{
	return g_gb[0]->get_cpu()->seri_send(dat);
}

static bool get_led()
{
	return (g_gb[0]->get_cregs()->RP&1)?true:false;
}

static bool have_child_menu(HMENU menu,int id)
{
	int cnt=0,sub;
	for(;;cnt++){
		sub=GetMenuItemID(menu,cnt);
		if ((sub==-1)&&(!GetSubMenu(menu,cnt)))
			return false;
		if ((sub!=-1)&&(sub==id))
			return true;
	}
	return false;
}

static HMENU search_menu(HMENU menu,int id)
{
	if (have_child_menu(menu,id))
		return menu;
	int cnt=0;
	HMENU sub;
	for(;;cnt++){
		if (sub=GetSubMenu(menu,cnt)){
			sub=search_menu(sub,id);
			if (sub) return sub;
		}
		else if (GetMenuItemID(menu,cnt)==-1)
			break;
	}
	return NULL;
}

static void init_devices()
{
	dev_callback.trash_device=trush_device;
	dev_callback.send_dat=send_dat;
	dev_callback.get_led=get_led;

	char buf[256],dev_dir[256],dev_name[256];
	WIN32_FIND_DATA wfd;
	HANDLE hSearch;
	HMODULE hDLL;
	void (*nmproc)(char*);
	int count=0;

	GetCurrentDirectory(256,buf);
	config->get_dev_dir(dev_dir);
	SetCurrentDirectory(dev_dir);

	hSearch=FindFirstFile("*.dll",&wfd);
	if (hSearch!=INVALID_HANDLE_VALUE){
		do{
			hDLL=LoadLibrary(wfd.cFileName);
			nmproc=(void(*)(char*))GetProcAddress(hDLL,"GetDeviceName");
			if (nmproc){
				nmproc(dev_name);
				sprintf(dll_dat[dev_count].file_name,"%s\\%s",dev_dir,wfd.cFileName);
				strcpy(dll_dat[dev_count++].device_name,dev_name);
				FreeLibrary(hDLL);
			}
		}while(FindNextFile(hSearch,&wfd));
		FindClose(hSearch);
	}

	SetCurrentDirectory(buf);

	HMENU hMenu=GetMenu(hWnd);
//	hMenu=GetSubMenu(hMenu,1);
//	hMenu=GetSubMenu(hMenu,6);
	hMenu=search_menu(hMenu,ID_ENVIRONMRNT);
	DeleteMenu(hMenu,0,MF_BYPOSITION);

	for (int i=0;i<dev_count;i++)
		AppendMenu(hMenu,MF_ENABLED,ID_ENVIRONMRNT+i,dll_dat[i].device_name);

	dev_loaded=false;
}

static bool create_device(char *name)
{
	bool (*init)(HWND hw,GBDEV_DAT*);

	if (hDevDll)
		FreeLibrary(hDevDll);
	hDevDll=LoadLibrary(name);
	ext_hook ext;

	init=(bool(*)(HWND hw,GBDEV_DAT*))GetProcAddress(hDevDll,"InitDevice");
	ext.send=(byte(*)(byte))GetProcAddress(hDevDll,"SerialTransfer");
	ext.led=(bool(*)())GetProcAddress(hDevDll,"GetInfraredLED");

	init(hWnd,&dev_callback);

	g_gb[0]->hook_extport(&ext);

	dev_loaded=true;
	return true;
}

static char tgb_help[64][256];

static void construct_help_menu(HMENU menu)
{
	char buf[256],hlp_dir[256],hf[256];
	WIN32_FIND_DATA wfd;
	HANDLE hSearch;
	int count=0;

	GetCurrentDirectory(256,buf);
	config->get_home_dir(hlp_dir);
	strcat(hlp_dir,"\\docs");
	SetCurrentDirectory(hlp_dir);

	sprintf(hf,"%s\\*.hlp",hlp_dir);
	hSearch=FindFirstFile(hf,&wfd);
	if (hSearch!=INVALID_HANDLE_VALUE){
		do{
			sprintf(tgb_help[count],"%s\\%s",hlp_dir,wfd.cFileName);
			AppendMenu(menu,MF_ENABLED,ID_TGBHELP+count++,wfd.cFileName);
		}while(FindNextFile(hSearch,&wfd));
		FindClose(hSearch);
	}
	sprintf(hf,"%s\\*.chm",hlp_dir);
	hSearch=FindFirstFile(hf,&wfd);
	if (hSearch!=INVALID_HANDLE_VALUE){
		do{
			sprintf(tgb_help[count],"%s\\%s",hlp_dir,wfd.cFileName);
			AppendMenu(menu,MF_ENABLED,ID_TGBHELP+count++,wfd.cFileName);
		}while(FindNextFile(hSearch,&wfd));
		FindClose(hSearch);
	}

	if (count){
		MENUITEMINFO mii;
		mii.cbSize=sizeof(mii);
		mii.fMask=MIIM_TYPE;
		mii.fType=MFT_SEPARATOR;
		InsertMenuItem(menu,count++,TRUE,&mii);
	}
	int bef_count=count;

	sprintf(hf,"%s\\*.txt",hlp_dir);
	hSearch=FindFirstFile(hf,&wfd);
	if (hSearch!=INVALID_HANDLE_VALUE){
		do{
			sprintf(tgb_help[count],"%s\\%s",hlp_dir,wfd.cFileName);
			AppendMenu(menu,MF_ENABLED,ID_TGBHELP+count++,wfd.cFileName);
		}while(FindNextFile(hSearch,&wfd));
		FindClose(hSearch);
	}

	if (bef_count!=count){
		MENUITEMINFO mii;
		mii.cbSize=sizeof(mii);
		mii.fMask=MIIM_TYPE;
		mii.fType=MFT_SEPARATOR;
		InsertMenuItem(menu,count,TRUE,&mii);
	}

	AppendMenu(menu,MF_ENABLED,ID_VERSION,"�o�[�W�������(&V)");

	SetCurrentDirectory(buf);
}

static BOOL CALLBACK TextProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

static void view_help(HWND hwnd,char *name)
{
	if (_mbsstr((BYTE*)name,(BYTE*)".hlp")||_mbsstr((BYTE*)name,(BYTE*)".HLP"))
		WinHelp(hwnd,name,HELP_INDEX,0);
	else if (_mbsstr((BYTE*)name,(BYTE*)".chm")||_mbsstr((BYTE*)name,(BYTE*)".CHM"))
		HtmlHelp(hwnd,name,HH_DISPLAY_TOC,0);
	else if (_mbsstr((BYTE*)name,(BYTE*)".txt")||_mbsstr((BYTE*)name,(BYTE*)".TXT")){
		FILE *file=fopen(name,"rb");
		fseek(file,0,SEEK_END);
		int size;
		char *dat=new char[(size=ftell(file))+1];
		fseek(file,0,SEEK_SET);
		fread(dat,1,size,file);
		fclose(file);
		dat[size]='\0';
		HWND text_hwnd;

		ShowWindow(text_hwnd=CreateDialog(hInstance,MAKEINTRESOURCE(IDD_TEXTVIEW),hwnd,TextProc),SW_SHOW);
		SendMessage(text_hwnd,WM_OUTLOG,0,(LPARAM)dat);
		delete []dat;
	}
}

//------------------------------------------------------------
// �R�}���h���C�����

static void purse_cmdline(char *cmdline)
{
	char buf[256],*p=cmdline;
	int i;

	while(*p){
		if ((*p=='-')||(*p=='/')){
			switch(*(p+1)){
			case 'f':
			case 'F':
				render[0]->swith_screen_mode();
				p+=2;
				break;
			}
		}
		else if (*p=='"'){
			p++;
			i=0;
			while(*p!='"')
				buf[i++]=*(p++);
			buf[i]='\0';

			char *pp,*q=NULL;
			pp=buf;

			for (i=0;pp[i]!='\0';i++)
				if (pp[i]=='\\')
					q=pp+i;
			if (q){
				*(q++)='\0';
				SetCurrentDirectory(pp);
			}
			else{
				char home_dir[256];
				config->get_home_dir(home_dir);
				SetCurrentDirectory(home_dir);
				q=buf;
			}

			load_rom(q,0);
			cur_mode=NORMAL_MODE;
			p++;
		}
		else if (*p==' '){
			p++;
		}
		else{
			i=0;
			while(*p!=' '&&*p!='\0')
				buf[i++]=*(p++);
			buf[i]='\0';

			char *pp,*q=NULL;
			pp=buf;

			for (i=0;pp[i]!='\0';i++)
				if (pp[i]=='\\')
					q=pp+i;
			if (q){
				*(q++)='\0';
				SetCurrentDirectory(pp);
			}
			else{
				char home_dir[256];
				config->get_home_dir(home_dir);
				SetCurrentDirectory(home_dir);
				q=buf;
			}

			load_rom(q,0);
			cur_mode=NORMAL_MODE;
		}
	}
}

//------------------------------------------------------------
// ���d�N���֎~

static HANDLE hMutex;

static bool pre_execute()
{
	if (OpenMutex(MUTEX_ALL_ACCESS,FALSE,"tgb mutex"))
		return true;
	else
		hMutex=CreateMutex(NULL,FALSE,"tgb mutex");
	return false;
}

static void trash_process()
{
	ReleaseMutex(hMutex);
}

//------------------------------------------------------------
// OS ���擾

static void os_check()
{
	OSVERSIONINFO ovi;
	ovi.dwOSVersionInfoSize=sizeof(ovi);
	GetVersionEx(&ovi);

	if (ovi.dwPlatformId==VER_PLATFORM_WIN32_NT)
		sys_win2000=true;
	else
		sys_win2000=false;
}

//------------------------------------------------------------
// �n���h���̗�

static BOOL CALLBACK VerProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		return TRUE;
	
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK LogProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	std::list<char*>::iterator ite;
	HWND hRich;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			CHARFORMAT frm={sizeof(frm)};
			hRich=GetDlgItem(hwnd,IDC_LOG);
			SendMessage( hRich, EM_SETREADONLY, TRUE, 0);
			SendMessage( hRich, EM_EXLIMITTEXT, 0, 655350);
//			SendMessage( hRich, EM_SETBKGNDCOLOR, 0, 0x00885555);
			SendMessage( hRich, EM_SETBKGNDCOLOR, 0, RGB(57,90,100));
			frm.dwMask=CFM_COLOR;
			frm.crTextColor=RGB(255,255,255);
			SendMessage( hRich, EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&frm);
			SendMessage( hRich, EM_SETMARGINS, EC_USEFONTINFO, 0);
		}
		for (ite=mes_list.begin();ite!=mes_list.end();ite++){
			DWORD TextLength = SendMessage( hRich, WM_GETTEXTLENGTH, 0, 0);
			SendMessage( hRich, EM_SETSEL, (WPARAM)TextLength, (LPARAM) TextLength);
			SendMessage( hRich, EM_REPLACESEL, FALSE, (LPARAM)*ite);
			SendMessage( hRich, EM_SCROLLCARET, 0, 0);
		}
		return TRUE;
	
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			mes_hwnd=NULL;
			DestroyWindow(hwnd);
			return TRUE;
		}
		break;
	case WM_OUTLOG:
		char *p;
		p=new char[256];
		strcpy(p,(char*)lParam);
		mes_list.push_back(p);

		hRich=GetDlgItem(hwnd,IDC_LOG);
		DWORD TextLength;
		TextLength = SendMessage( hRich, WM_GETTEXTLENGTH, 0, 0);
		SendMessage( hRich, EM_SETSEL, (WPARAM)TextLength, (LPARAM) TextLength);
		SendMessage( hRich, EM_REPLACESEL, FALSE, (LPARAM)lParam);
		SendMessage( hRich, EM_SCROLLCARET, 0, 0);
		break;
	case WM_CLOSE:
		mes_hwnd=NULL;
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK TextProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	HWND hRich;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		CHARFORMAT frm;
		hRich=GetDlgItem(hwnd,IDC_VIEWHELP);
		BYTE r,g,b;
		r=rand()%90;
		g=rand()%80;
		b=rand()%85;
		SendMessage( hRich, EM_SETREADONLY, TRUE, 0);
		SendMessage( hRich, EM_EXLIMITTEXT, 0, 655350);
		SendMessage( hRich, EM_SETBKGNDCOLOR, 0, RGB(255-r,255-g,255-b));
		frm.cbSize=sizeof(frm);
		frm.dwMask=CFM_COLOR;
		frm.crTextColor=RGB(r,g,b);
		SendMessage( hRich, EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&frm);
		SendMessage( hRich, EM_SETMARGINS, EC_USEFONTINFO, 0);
		return TRUE;
	
	case WM_OUTLOG:
		SetDlgItemText(hwnd,IDC_VIEWHELP,(LPCTSTR)lParam);
		return TRUE;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK KeyProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static const int map[]={0,1,3,2,5,4,6,7,8,9,11,10,13,12,14,15};
	int key[32];
	int dev;
	char buf[20];
	int pad_id,pad_dir;
	int i;
	WORD any;
	HWND htmp;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendMessage(hwnd,WM_USER+1,0,0);
		CheckDlgButton(hwnd,IDC_FFB,config->use_ffb?BST_CHECKED:BST_UNCHECKED);
		SetTimer(hwnd,777,25,NULL);
		return TRUE;
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			KillTimer(hwnd,777);
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_FFB){
			if (IsDlgButtonChecked(hwnd,IDC_FFB)==BST_CHECKED){
				config->use_ffb=true;
				render[0]->set_use_ffb(true);
			}
			else{
				config->use_ffb=false;
				render[0]->set_use_ffb(false);
			}
			return TRUE;
		}
		break;
	case WM_CLOSE:
		KillTimer(hwnd,777);
		DestroyWindow(hwnd);
		return TRUE;
	case WM_USER+1:
	{
		config->get_key_setting(&key[0],0);
		config->get_key_setting(&key[16],1);

		for (i=0;i<16;i++){
			dev=key[map[i]*2];
			if (dev==DI_KEYBOARD)
				sprintf(buf,"%s",keyboad_map[key[map[i]*2+1]]);
			else if (dev==DI_MOUSE_X)
				strcpy(buf,key[map[i]*2+1]?"Mouse -X":"Mouse +X");
			else if (dev==DI_MOUSE_Y)
				strcpy(buf,key[map[i]*2+1]?"Mouse -Y":"Mouse +Y");
			else if (dev==DI_MOUSE)
				sprintf(buf,"Mouse %d",key[map[i]*2+1]);
			else{
				pad_id=(dev-DI_PAD_X)/NEXT_PAD;
				pad_dir=(dev-DI_PAD_X)%NEXT_PAD;
				if (pad_dir==0)
					sprintf(buf,"Pad%d %s",pad_id,key[map[i]*2+1]?"-X":"+X");
				else if (pad_dir==1)
					sprintf(buf,"Pad%d %s",pad_id,key[map[i]*2+1]?"-Y":"+Y");
				else
					sprintf(buf,"Pad%d %d",pad_id,key[map[i]*2+1]);
			}
			SetDlgItemText(hwnd,IDC_1A+i,buf);
		}

		int tmp_type[5]={config->fast_forwerd[0],config->save_key[0],config->load_key[0],config->auto_key[0],config->pause_key[0]};
		int tmp_code[5]={config->fast_forwerd[1],config->save_key[1],config->load_key[1],config->auto_key[1],config->pause_key[1]};

		for (i=0;i<5;i++){
			dev=tmp_type[i];
			if (dev==DI_KEYBOARD)
				sprintf(buf,"%s",keyboad_map[tmp_code[i]]);
			else if (dev==DI_MOUSE_X)
				strcpy(buf,tmp_code[i]?"Mouse -X":"Mouse +X");
			else if (dev==DI_MOUSE_Y)
				strcpy(buf,tmp_code[i]?"Mouse -Y":"Mouse +Y");
			else if (dev==DI_MOUSE)
				sprintf(buf,"Mouse %d",tmp_code[i]);
			else{
				pad_id=(dev-DI_PAD_X)/NEXT_PAD;
				pad_dir=(dev-DI_PAD_X)%NEXT_PAD;
				if (pad_dir==0)
					sprintf(buf,"Pad%d %s",pad_id,tmp_code[i]?"-X":"+X");
				else if (pad_dir==1)
					sprintf(buf,"Pad%d %s",pad_id,tmp_code[i]?"-Y":"+Y");
				else
					sprintf(buf,"Pad%d %d",pad_id,tmp_code[i]);
			}
			SetDlgItemText(hwnd,IDC_1A+16+i,buf);
		}

		break;
	}
	case WM_TIMER:
		any=render[0]->get_any_key();
		if (any){
			htmp=GetFocus();

			for (i=0;i<21;i++)
				if (htmp==GetDlgItem(hwnd,IDC_1A+i))
					break;
			if (i<16){
				if (i<8){
					config->get_key_setting(&key[0],0);
					key[map[i]*2]=any>>8;
					key[map[i]*2+1]=any&0xff;
					config->set_key_setting(key,0);
					load_key_config(0);
					SendMessage(hwnd,WM_USER+1,0,0);
				}
				else{
					i-=8;
					config->get_key_setting(&key[0],1);
					key[map[i]*2]=any>>8;
					key[map[i]*2+1]=any&0xff;
					config->set_key_setting(key,1);
					if (render[1])
						load_key_config(1);
					SendMessage(hwnd,WM_USER+1,0,0);
					i+=8;
				}
				i=(i+1)%16;
				SetFocus(GetDlgItem(hwnd,IDC_1A+i));
			}
			else if (i!=21){
				if (i==16){
					config->fast_forwerd[0]=any>>8;
					config->fast_forwerd[1]=any&0xff;
					i=17;
					SetFocus(GetDlgItem(hwnd,IDC_1A+i));
					SendMessage(hwnd,WM_USER+1,0,0);
				}
				else if (i==17){
					config->save_key[0]=any>>8;
					config->save_key[1]=any&0xff;
					key_dat tmp_key;
					tmp_key.device_type=config->save_key[0];
					tmp_key.key_code=config->save_key[1];
					render[0]->set_save_key(&tmp_key);
					i=18;
					SetFocus(GetDlgItem(hwnd,IDC_1A+i));
					SendMessage(hwnd,WM_USER+1,0,0);
				}
				else if (i==18){
					config->load_key[0]=any>>8;
					config->load_key[1]=any&0xff;
					key_dat tmp_key;
					tmp_key.device_type=config->load_key[0];
					tmp_key.key_code=config->load_key[1];
					render[0]->set_load_key(&tmp_key);
					i=19;
					SetFocus(GetDlgItem(hwnd,IDC_1A+i));
					SendMessage(hwnd,WM_USER+1,0,0);
				}
				else if (i==19){
					config->auto_key[0]=any>>8;
					config->auto_key[1]=any&0xff;
					key_dat tmp_key;
					tmp_key.device_type=config->auto_key[0];
					tmp_key.key_code=config->auto_key[1];
					render[0]->set_auto_key(&tmp_key);
					i=20;
					SetFocus(GetDlgItem(hwnd,IDC_1A+i));
					SendMessage(hwnd,WM_USER+1,0,0);
				}
				else if (i==20){
					config->pause_key[0]=any>>8;
					config->pause_key[1]=any&0xff;
					key_dat tmp_key;
					tmp_key.device_type=config->pause_key[0];
					tmp_key.key_code=config->pause_key[1];
					render[0]->set_pause_key(&tmp_key);
					i=16;
					SetFocus(GetDlgItem(hwnd,IDC_1A+i));
					SendMessage(hwnd,WM_USER+1,0,0);
				}
			}
			Sleep(200);
		}
		break;
	}

    return FALSE;
}

static BOOL CALLBACK DirectoryProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static char cur_sv_dir[256];
	static char cur_md_dir[256];

	switch(uMsg)
	{
	case WM_INITDIALOG:
		config->get_save_dir(cur_sv_dir);
		config->get_media_dir(cur_md_dir);
		SetDlgItemText(hwnd,IDC_SAVE_PATH,cur_sv_dir);
		SetDlgItemText(hwnd,IDC_MEDIA_PATH,cur_md_dir);
		return TRUE;
	
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			config->set_save_dir(cur_sv_dir);
			config->set_media_dir(cur_md_dir);
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if(LOWORD(wParam)==IDC_SAVE_DIR){
			char sv_nm[256];
			BROWSEINFO bi;
			LPITEMIDLIST pidl;
			bi.hwndOwner=NULL;
			bi.pidlRoot=NULL;
			bi.pszDisplayName=sv_nm;
			bi.lpszTitle="";
			bi.ulFlags=BIF_RETURNONLYFSDIRS;
			bi.lpfn=NULL;
			bi.lParam=0;
			bi.iImage=0;
			if ((pidl=SHBrowseForFolder(&bi))!=NULL){
				SHGetPathFromIDList(pidl,cur_sv_dir);
				SetDlgItemText(hwnd,IDC_SAVE_PATH,cur_sv_dir);
				CoTaskMemFree(pidl);
			}
			return TRUE;
		}
		else if(LOWORD(wParam)==IDC_MEDIA_DIR){
			char md_nm[256];
			BROWSEINFO bi;
			LPITEMIDLIST pidl;
			bi.hwndOwner=NULL;
			bi.pidlRoot=NULL;
			bi.pszDisplayName=md_nm;
			bi.lpszTitle="";
			bi.ulFlags=BIF_RETURNONLYFSDIRS;
			bi.lpfn=NULL;
			bi.lParam=0;
			bi.iImage=0;
			if ((pidl=SHBrowseForFolder(&bi))!=NULL){
				SHGetPathFromIDList(pidl,cur_md_dir);
				SetDlgItemText(hwnd,IDC_MEDIA_PATH,cur_md_dir);
				CoTaskMemFree(pidl);
			}
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

	return FALSE;
}

static BOOL CALLBACK FilterProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendMessage(hwnd,WM_USER+1,0,0);
		return TRUE;
	
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_REAL){
			config->r_def=config->g_def=config->b_def=0;
			config->r_div=config->g_div=config->b_div=256;
			config->r_r=config->g_g=config->b_b=256;
			config->r_g=config->g_b=config->b_r=config->r_b=config->g_r=config->b_g=0;
			SendMessage(hwnd,WM_USER+1,0,0);
		}
		else if (LOWORD(wParam)==IDC_LCD){
			config->r_def=20;
			config->g_def=48;
			config->b_def=45;
			config->r_div=config->g_div=config->b_div=512;
			config->r_r=382;
			config->r_g=71;
			config->r_b=-5;
			config->g_r=20;
			config->g_g=236;
			config->g_b=101;
			config->b_r=57;
			config->b_g=52;
			config->b_b=154;
			SendMessage(hwnd,WM_USER+1,0,0);
		}
		else{
			SendMessage(hwnd,WM_USER+2,0,0);
		}
		break;
	case WM_USER+1:
		int r_def,g_def,b_def;
		int r_div,g_div,b_div;
		int r_r,r_g,r_b;
		int g_r,g_g,g_b;
		int b_r,b_g,b_b;

		r_def=config->r_def;
		g_def=config->g_def;
		b_def=config->b_def;
		r_div=config->r_div;
		g_div=config->g_div;
		b_div=config->b_div;
		r_r=config->r_r;
		r_g=config->r_g;
		r_b=config->r_b;
		g_r=config->g_r;
		g_g=config->g_g;
		g_b=config->g_b;
		b_r=config->b_r;
		b_g=config->b_g;
		b_b=config->b_b;

		SetDlgItemInt(hwnd,IDC_RDEF,r_def,TRUE);
		SetDlgItemInt(hwnd,IDC_GDEF,g_def,TRUE);
		SetDlgItemInt(hwnd,IDC_BDEF,b_def,TRUE);
		SetDlgItemInt(hwnd,IDC_RDIV,r_div,TRUE);
		SetDlgItemInt(hwnd,IDC_GDIV,g_div,TRUE);
		SetDlgItemInt(hwnd,IDC_BDIV,b_div,TRUE);
		SetDlgItemInt(hwnd,IDC_RR,r_r,TRUE);
		SetDlgItemInt(hwnd,IDC_RG,r_g,TRUE);
		SetDlgItemInt(hwnd,IDC_RB,r_b,TRUE);
		SetDlgItemInt(hwnd,IDC_GR,g_r,TRUE);
		SetDlgItemInt(hwnd,IDC_GG,g_g,TRUE);
		SetDlgItemInt(hwnd,IDC_GB,g_b,TRUE);
		SetDlgItemInt(hwnd,IDC_BR,b_r,TRUE);
		SetDlgItemInt(hwnd,IDC_BG,b_g,TRUE);
		SetDlgItemInt(hwnd,IDC_BB,b_b,TRUE);

		load_key_config(0);
		if (render[1])
			load_key_config(1);
		break;
	case WM_USER+2:
		config->r_def=GetDlgItemInt(hwnd,IDC_RDEF,NULL,TRUE);
		config->g_def=GetDlgItemInt(hwnd,IDC_GDEF,NULL,TRUE);
		config->b_def=GetDlgItemInt(hwnd,IDC_BDEF,NULL,TRUE);
		config->r_div=GetDlgItemInt(hwnd,IDC_RDIV,NULL,TRUE);
		config->g_div=GetDlgItemInt(hwnd,IDC_GDIV,NULL,TRUE);
		config->b_div=GetDlgItemInt(hwnd,IDC_BDIV,NULL,TRUE);
		config->r_r=GetDlgItemInt(hwnd,IDC_RR,NULL,TRUE);
		config->r_g=GetDlgItemInt(hwnd,IDC_RG,NULL,TRUE);
		config->r_b=GetDlgItemInt(hwnd,IDC_RB,NULL,TRUE);
		config->g_r=GetDlgItemInt(hwnd,IDC_GR,NULL,TRUE);
		config->g_g=GetDlgItemInt(hwnd,IDC_GG,NULL,TRUE);
		config->g_b=GetDlgItemInt(hwnd,IDC_GB,NULL,TRUE);
		config->b_r=GetDlgItemInt(hwnd,IDC_BR,NULL,TRUE);
		config->b_g=GetDlgItemInt(hwnd,IDC_BG,NULL,TRUE);
		config->b_b=GetDlgItemInt(hwnd,IDC_BB,NULL,TRUE);

		load_key_config(0);
		int i;
		if (g_gb[0])
			for (i=0;i<64;i++)
				g_gb[0]->get_lcd()->get_mapped_pal(i>>2)[i&3]=g_gb[0]->get_renderer()->map_color(g_gb[0]->get_lcd()->get_pal(i>>2)[i&3]);

		if (render[1])
			load_key_config(1);
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK KorokoroProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		EnableWindow(GetDlgItem(hwnd,IDC_SPLIT),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_SKUP),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_SKDOWN),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_SKLEFT),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_SKRIGHT),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_KUP),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_KDOWN),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_KLEFT),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_KRIGHT),!config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_SENSITIVE),config->koro_use_analog);
		EnableWindow(GetDlgItem(hwnd,IDC_SS),config->koro_use_analog);

		CheckDlgButton(hwnd,IDC_USE_ANALOG,config->koro_use_analog?BST_CHECKED:BST_UNCHECKED);
		CheckDlgButton(hwnd,IDC_UNUSE,config->koro_use_analog?BST_UNCHECKED:BST_CHECKED);

		SendMessage(hwnd,WM_USER+1,0,0);
		SetTimer(hwnd,888,25,NULL);

		return TRUE;

	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			if (config->koro_use_analog){
				config->koro_sensitive=GetDlgItemInt(hwnd,IDC_SENSITIVE,NULL,FALSE);
				load_key_config(0);
			}
			KillTimer(hwnd,888);
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_USE_ANALOG){
			config->koro_use_analog=true;
			EnableWindow(GetDlgItem(hwnd,IDC_SPLIT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKUP),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKDOWN),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKLEFT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKRIGHT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KUP),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KDOWN),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KLEFT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KRIGHT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SENSITIVE),config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SS),config->koro_use_analog);
		}
		else if (LOWORD(wParam)==IDC_UNUSE){
			config->koro_use_analog=false;
			EnableWindow(GetDlgItem(hwnd,IDC_SPLIT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKUP),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKDOWN),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKLEFT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SKRIGHT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KUP),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KDOWN),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KLEFT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_KRIGHT),!config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SENSITIVE),config->koro_use_analog);
			EnableWindow(GetDlgItem(hwnd,IDC_SS),config->koro_use_analog);
		}
		break;
	case WM_TIMER:
		int any,i;
		HWND htmp;

		any=render[0]->get_any_key();
		if (any){
			htmp=GetFocus();

			for (i=0;i<4;i++)
				if (htmp==GetDlgItem(hwnd,IDC_KUP+i))
					break;
			if (i==4)
				break;
			config->koro_key[i*2]=any>>8;
			config->koro_key[i*2+1]=any&0xff;
			load_key_config(0);
			i=(i+1)&3;
			SetFocus(GetDlgItem(hwnd,IDC_KUP+i));
			SendMessage(hwnd,WM_USER+1,0,0);
			Sleep(200);
		}
		break;
	case WM_USER+1:
	{
		int i,dev,pad_id,pad_dir;
		char buf[16];

		for (i=0;i<4;i++){
			dev=config->koro_key[i*2];
			if (dev==DI_KEYBOARD)
				sprintf(buf,"%s",keyboad_map[config->koro_key[i*2+1]]);
			else if (dev==DI_MOUSE_X)
				strcpy(buf,config->koro_key[i*2+1]?"Mouse -X":"Mouse +X");
			else if (dev==DI_MOUSE_Y)
				strcpy(buf,config->koro_key[i*2+1]?"Mouse -Y":"Mouse +Y");
			else if (dev==DI_MOUSE)
				sprintf(buf,"Mouse %d",config->koro_key[i*2+1]);
			else{
				pad_id=(dev-DI_PAD_X)/NEXT_PAD;
				pad_dir=(dev-DI_PAD_X)%NEXT_PAD;
				if (pad_dir==0)
					sprintf(buf,"Pad%d %s",pad_id,config->koro_key[i*2+1]?"-X":"+X");
				else if (pad_dir==1)
					sprintf(buf,"Pad%d %s",pad_id,config->koro_key[i*2+1]?"-Y":"+Y");
				else
					sprintf(buf,"Pad%d %d",pad_id,config->koro_key[i*2+1]);
			}
			SetDlgItemText(hwnd,IDC_KUP+i,buf);
		}

		SetDlgItemInt(hwnd,IDC_SENSITIVE,config->koro_sensitive,TRUE);

		break;
	}
	case WM_CLOSE:
		KillTimer(hwnd,888);
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK ClockProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	int day,hour,min,sec;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendMessage(hwnd,WM_USER+1,0,0);
		SetTimer(hwnd,969,1000,NULL);
		return TRUE;
	case WM_COMMAND:
		if ((LOWORD(wParam)==IDOK)||(LOWORD(wParam)==IDCANCEL)){
			day=GetDlgItemInt(hwnd,IDC_DAY,NULL,FALSE);
			hour=GetDlgItemInt(hwnd,IDC_HOUR,NULL,FALSE);
			min=GetDlgItemInt(hwnd,IDC_MIN,NULL,FALSE);
			sec=GetDlgItemInt(hwnd,IDC_SEC,NULL,FALSE);
			render[0]->set_time(12,(BYTE)((day>>8)&1));
			render[0]->set_time(11,(BYTE)(day&0xff));
			render[0]->set_time(10,(BYTE)hour);
			render[0]->set_time(9,(BYTE)min);
			render[0]->set_time(8,(BYTE)sec);

			if (LOWORD(wParam)==IDCANCEL)
				DestroyWindow(hwnd);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	case WM_TIMER:
		HWND hFocus;
		hFocus=GetFocus();
		if ((hFocus==GetDlgItem(hwnd,IDC_DAY))||(hFocus==GetDlgItem(hwnd,IDC_HOUR))||(hFocus==GetDlgItem(hwnd,IDC_MIN))||(hFocus==GetDlgItem(hwnd,IDC_SEC)))
			return FALSE;
	case WM_USER+1:
		day=((render[0]->get_time(12)&1)<<8)|(render[0]->get_time(11)&0xff);
		hour=render[0]->get_time(10)&0xff;
		min=render[0]->get_time(9)&0xff;
		sec=render[0]->get_time(8)&0xff;

		SetDlgItemInt(hwnd,IDC_DAY,day,FALSE);
		SetDlgItemInt(hwnd,IDC_HOUR,hour,FALSE);
		SetDlgItemInt(hwnd,IDC_MIN,min,FALSE);
		SetDlgItemInt(hwnd,IDC_SEC,sec,FALSE);
		return FALSE;
	}

    return FALSE;
}

static BOOL CALLBACK SoundProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		if ((!g_gb[0])&&(!g_gbr))
			break;
/*
		CheckDlgButton(hwnd,IDC_SQ1,(g_gb[0]->get_apu()->get_renderer()->get_enable(0)?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_SQ2,(g_gb[0]->get_apu()->get_renderer()->get_enable(1)?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_WAV,(g_gb[0]->get_apu()->get_renderer()->get_enable(2)?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_NOI,(g_gb[0]->get_apu()->get_renderer()->get_enable(3)?BST_CHECKED:BST_UNCHECKED));
*/
		CheckDlgButton(hwnd,IDC_SQ1,(config->sound_enable[0]?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_SQ2,(config->sound_enable[1]?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_WAV,(config->sound_enable[2]?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_NOI,(config->sound_enable[3]?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_MASTER,(config->sound_enable[4]?BST_CHECKED:BST_UNCHECKED));

		if (config->sound_enable[4]){
			EnableWindow(GetDlgItem(hwnd,IDC_SQ1),TRUE);
			EnableWindow(GetDlgItem(hwnd,IDC_SQ2),TRUE);
			EnableWindow(GetDlgItem(hwnd,IDC_WAV),TRUE);
			EnableWindow(GetDlgItem(hwnd,IDC_NOI),TRUE);
		}
		else{
			EnableWindow(GetDlgItem(hwnd,IDC_SQ1),FALSE);
			EnableWindow(GetDlgItem(hwnd,IDC_SQ2),FALSE);
			EnableWindow(GetDlgItem(hwnd,IDC_WAV),FALSE);
			EnableWindow(GetDlgItem(hwnd,IDC_NOI),FALSE);
		}

		CheckDlgButton(hwnd,IDC_ECHO,(config->b_echo?BST_CHECKED:BST_UNCHECKED));
		CheckDlgButton(hwnd,IDC_LOWPASS,(config->b_lowpass?BST_CHECKED:BST_UNCHECKED));
		return TRUE;
	
	case WM_COMMAND:
		if ((!g_gb[0])&&(!g_gbr))
			break;
		if(LOWORD(wParam)==IDC_SQ1){
			config->sound_enable[0]=(IsDlgButtonChecked(hwnd,IDC_SQ1)==BST_CHECKED)?1:0;
			if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(0,config->sound_enable[0]?true:false);
			if (g_gbr) g_gbr->set_enable(0,config->sound_enable[0]?true:false);
		}
		else if(LOWORD(wParam)==IDC_SQ2){
			config->sound_enable[1]=(IsDlgButtonChecked(hwnd,IDC_SQ2)==BST_CHECKED)?1:0;
			if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(1,config->sound_enable[1]?true:false);
			if (g_gbr) g_gbr->set_enable(1,config->sound_enable[1]?true:false);
		}
		else if(LOWORD(wParam)==IDC_WAV){
			config->sound_enable[2]=(IsDlgButtonChecked(hwnd,IDC_WAV)==BST_CHECKED)?1:0;
			if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(2,config->sound_enable[2]?true:false);
			if (g_gbr) g_gbr->set_enable(2,config->sound_enable[2]?true:false);
		}
		else if(LOWORD(wParam)==IDC_NOI){
			config->sound_enable[3]=(IsDlgButtonChecked(hwnd,IDC_NOI)==BST_CHECKED)?1:0;
			if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(3,config->sound_enable[3]?true:false);
			if (g_gbr) g_gbr->set_enable(3,config->sound_enable[3]?true:false);
		}
		else if(LOWORD(wParam)==IDC_MASTER){
			config->sound_enable[4]=(IsDlgButtonChecked(hwnd,IDC_MASTER)==BST_CHECKED)?1:0;
			if (config->sound_enable[4]){
				EnableWindow(GetDlgItem(hwnd,IDC_SQ1),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_SQ2),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_WAV),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_NOI),TRUE);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(0,config->sound_enable[0]?true:false);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(1,config->sound_enable[1]?true:false);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(2,config->sound_enable[2]?true:false);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(3,config->sound_enable[3]?true:false);
				if (g_gbr) g_gbr->set_enable(0,config->sound_enable[0]?true:false);
				if (g_gbr) g_gbr->set_enable(1,config->sound_enable[1]?true:false);
				if (g_gbr) g_gbr->set_enable(2,config->sound_enable[2]?true:false);
				if (g_gbr) g_gbr->set_enable(3,config->sound_enable[3]?true:false);
			}
			else{
				EnableWindow(GetDlgItem(hwnd,IDC_SQ1),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_SQ2),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_WAV),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_NOI),FALSE);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(0,false);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(1,false);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(2,false);
				if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_enable(3,false);
				if (g_gbr) g_gbr->set_enable(0,false);
				if (g_gbr) g_gbr->set_enable(1,false);
				if (g_gbr) g_gbr->set_enable(2,false);
				if (g_gbr) g_gbr->set_enable(3,false);
			}
		}
		else if(LOWORD(wParam)==IDC_ECHO){
			config->b_echo=(IsDlgButtonChecked(hwnd,IDC_ECHO)==BST_CHECKED);
			if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_echo(config->b_echo);
			if (g_gbr) g_gbr->set_effect(1,config->b_echo);
		}
		else if(LOWORD(wParam)==IDC_LOWPASS){
			config->b_lowpass=(IsDlgButtonChecked(hwnd,IDC_LOWPASS)==BST_CHECKED);
			if (g_gb[0]) g_gb[0]->get_apu()->get_renderer()->set_lowpass(config->b_lowpass);
			if (g_gbr) g_gbr->set_effect(0,config->b_lowpass);
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK SpeedProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	char buf[256];

	switch(uMsg)
	{
	case WM_INITDIALOG:
		sprintf(buf,"%d",config->frame_skip);
		SetDlgItemText(hwnd,IDC_SKIP,buf);
		sprintf(buf,"%d",config->virtual_fps);
		SetDlgItemText(hwnd,IDC_FPS,buf);
		sprintf(buf,"%d",config->fast_frame_skip);
		SetDlgItemText(hwnd,IDC_SKIP2,buf);
		sprintf(buf,"%d",config->fast_virtual_fps);
		SetDlgItemText(hwnd,IDC_FPS2,buf);
		if (!config->speed_limit){
			CheckDlgButton(hwnd,IDC_LIMITTER,BST_CHECKED);
			EnableWindow(GetDlgItem(hwnd,IDC_FPS),FALSE);
		}
		if (!config->fast_speed_limit){
			CheckDlgButton(hwnd,IDC_LIMITTER2,BST_CHECKED);
			EnableWindow(GetDlgItem(hwnd,IDC_FPS2),FALSE);
		}
		if (config->show_fps)
			CheckDlgButton(hwnd,IDC_SHOWFPS,BST_CHECKED);
		return TRUE;
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_SKIP){
			GetDlgItemText(hwnd,IDC_SKIP,buf,256);
			config->frame_skip=strtol(buf,NULL,10);
		}
		else if (LOWORD(wParam)==IDC_FPS){
			GetDlgItemText(hwnd,IDC_FPS,buf,256);
			config->virtual_fps=strtol(buf,NULL,10);
			config->virtual_fps=(config->virtual_fps>0)?config->virtual_fps:1;
		}
		else if (LOWORD(wParam)==IDC_SKIP2){
			GetDlgItemText(hwnd,IDC_SKIP2,buf,256);
			config->fast_frame_skip=strtol(buf,NULL,10);
		}
		else if (LOWORD(wParam)==IDC_FPS2){
			GetDlgItemText(hwnd,IDC_FPS2,buf,256);
			config->fast_virtual_fps=strtol(buf,NULL,10);
			config->fast_virtual_fps=(config->fast_virtual_fps>0)?config->fast_virtual_fps:1;
		}
		else if (LOWORD(wParam)==IDC_LIMITTER){
			config->speed_limit=(IsDlgButtonChecked(hwnd,IDC_LIMITTER)!=BST_CHECKED);
			if (!config->speed_limit)
				EnableWindow(GetDlgItem(hwnd,IDC_FPS),FALSE);
			else
				EnableWindow(GetDlgItem(hwnd,IDC_FPS),TRUE);
		}
		else if (LOWORD(wParam)==IDC_LIMITTER2){
			config->fast_speed_limit=(IsDlgButtonChecked(hwnd,IDC_LIMITTER2)!=BST_CHECKED);
			if (!config->fast_speed_limit)
				EnableWindow(GetDlgItem(hwnd,IDC_FPS2),FALSE);
			else
				EnableWindow(GetDlgItem(hwnd,IDC_FPS2),TRUE);
		}
		else if (LOWORD(wParam)==IDC_SHOWFPS){
			config->show_fps=(IsDlgButtonChecked(hwnd,IDC_SHOWFPS)==BST_CHECKED);
			render[0]->show_fps(config->show_fps);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK MemProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static WORD cur_adr;
	HWND hScrl;
	HWND hList;
	SCROLLINFO scr;
	char buf[256];
	int tmp,i;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		cur_adr=0xC000;

		scr.cbSize=sizeof(scr);
		scr.fMask=SIF_RANGE|SIF_POS;
		scr.nMin=0;
		scr.nMax=0xFFF-0x9;
		scr.nPos=cur_adr>>4;
		hScrl=GetDlgItem(hwnd,IDC_SCROLL);
		SetScrollInfo(hScrl,SB_CTL,&scr,TRUE);
		tmp=scr.nPos;
		SendMessage(hwnd,WM_USER+1,(WPARAM)tmp,0);

		SetTimer(hwnd,789,100,NULL);

		return TRUE;
	
	case WM_VSCROLL:

		int max,min;

		hScrl=GetDlgItem(hwnd,IDC_SCROLL);
		max=0xFFF-0x9;
		min=0;
		hScrl=(HWND)lParam;
		scr.cbSize=sizeof(scr);
		scr.fMask=SIF_POS;
		GetScrollInfo(hScrl,SB_CTL,&scr);
		switch(LOWORD(wParam)){
		case SB_BOTTOM:
			scr.nPos=max;
			break;
		case SB_LINEDOWN:
			scr.nPos++;
			break;
		case SB_PAGEDOWN:
			scr.nPos+=10;
			break;
		case SB_LINEUP:
			scr.nPos--;
			break;
		case SB_PAGEUP:
			scr.nPos-=10;
			break;
		case SB_TOP:
			scr.nPos=min;
			break;
		case SB_ENDSCROLL:
			break;
		default:
			scr.nPos=(short)HIWORD(wParam);
			break;
		}
		if (scr.nPos>max) scr.nPos=max;
		if (scr.nPos<min) scr.nPos=min;

		tmp=scr.nPos;

		SetScrollInfo(hScrl,SB_CTL,&scr,TRUE);

		SendMessage(hwnd,WM_USER+1,(WPARAM)tmp,0);

		break;
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if(LOWORD(wParam)==IDC_MOVE){
			GetDlgItemText(hwnd,IDC_ADDRESS,buf,256);
			tmp=(strtol(buf,NULL,16)>>4)&0xFFF;
			if (tmp>0xFF6) tmp=0xFF6;
			hScrl=GetDlgItem(hwnd,IDC_SCROLL);
			SetScrollPos(hScrl,SB_CTL,tmp,TRUE);
			SendMessage(hwnd,WM_USER+1,(WPARAM)tmp,0);

			return TRUE;
		}
		break;
	case WM_USER+1:
		tmp=(int)wParam;
		hList=GetDlgItem(hwnd,IDC_MEMORY);
		if (SendMessage(hList,LB_GETTEXTLEN,0,0)!=LB_ERR)
		while(SendMessage(hList,LB_DELETESTRING,0,0));

		strcpy(buf,"        00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
		SendMessage(hList,LB_INSERTSTRING,0,(LPARAM)buf);

		for (i=0;i<10;i++){
			sprintf(buf,"[%04X]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",tmp<<4,
				g_gb[0]->get_cpu()->read(((tmp)<<4)+0),g_gb[0]->get_cpu()->read(((tmp)<<4)+1),
				g_gb[0]->get_cpu()->read(((tmp)<<4)+2),g_gb[0]->get_cpu()->read(((tmp)<<4)+3),
				g_gb[0]->get_cpu()->read(((tmp)<<4)+4),g_gb[0]->get_cpu()->read(((tmp)<<4)+5),
				g_gb[0]->get_cpu()->read(((tmp)<<4)+6),g_gb[0]->get_cpu()->read(((tmp)<<4)+7),	
				g_gb[0]->get_cpu()->read(((tmp)<<4)+8),g_gb[0]->get_cpu()->read(((tmp)<<4)+9),
				g_gb[0]->get_cpu()->read(((tmp)<<4)+10),g_gb[0]->get_cpu()->read(((tmp)<<4)+11),
				g_gb[0]->get_cpu()->read(((tmp)<<4)+12),g_gb[0]->get_cpu()->read(((tmp)<<4)+13),
				g_gb[0]->get_cpu()->read(((tmp)<<4)+14),g_gb[0]->get_cpu()->read(((tmp)<<4)+15));
			SendMessage(hList,LB_INSERTSTRING,1+i,(LPARAM)buf);
			tmp++;
		}
		break;
	case WM_TIMER:
		hScrl=GetDlgItem(hwnd,IDC_SCROLL);
		scr.cbSize=sizeof(scr);
		scr.fMask=SIF_POS;
		GetScrollInfo(hScrl,SB_CTL,&scr);
		SendMessage(hwnd,WM_USER+1,(WPARAM)scr.nPos,0);
		break;
	case WM_CLOSE:
		KillTimer(hwnd,789);
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK NoMemProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static WORD cur_adr;
	HWND hScrl;
	HWND hList;
	SCROLLINFO scr;
	char buf[256];
	int tmp,i;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		cur_adr=0x0000;

		scr.cbSize=sizeof(scr);
		scr.fMask=SIF_RANGE|SIF_POS;
		scr.nMin=0x100;
		scr.nMax=0x7FF-0x9;
		scr.nPos=cur_adr>>4;
		hScrl=GetDlgItem(hwnd,IDC_SCROLL);
		SetScrollInfo(hScrl,SB_CTL,&scr,TRUE);
		tmp=scr.nPos;
		SendMessage(hwnd,WM_USER+1,(WPARAM)tmp,0);

		SetTimer(hwnd,789,100,NULL);

		return TRUE;
	
	case WM_VSCROLL:

		int max,min;

		hScrl=GetDlgItem(hwnd,IDC_SCROLL);
		max=0x7FF-0x9;
		min=0x100;
		hScrl=(HWND)lParam;
		scr.cbSize=sizeof(scr);
		scr.fMask=SIF_POS;
		GetScrollInfo(hScrl,SB_CTL,&scr);
		switch(LOWORD(wParam)){
		case SB_BOTTOM:
			scr.nPos=max;
			break;
		case SB_LINEDOWN:
			scr.nPos++;
			break;
		case SB_PAGEDOWN:
			scr.nPos+=10;
			break;
		case SB_LINEUP:
			scr.nPos--;
			break;
		case SB_PAGEUP:
			scr.nPos-=10;
			break;
		case SB_TOP:
			scr.nPos=min;
			break;
		case SB_ENDSCROLL:
			break;
		default:
			scr.nPos=(short)HIWORD(wParam);
			break;
		}
		if (scr.nPos>max) scr.nPos=max;
		if (scr.nPos<min) scr.nPos=min;

		tmp=scr.nPos;

		SetScrollInfo(hScrl,SB_CTL,&scr,TRUE);

		SendMessage(hwnd,WM_USER+1,(WPARAM)tmp,0);

		break;
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if(LOWORD(wParam)==IDC_MOVE){
			GetDlgItemText(hwnd,IDC_ADDRESS,buf,256);
			tmp=(strtol(buf,NULL,16)>>4)&0x7FF;
			if (tmp>0x7F6) tmp=0x7F6;
			hScrl=GetDlgItem(hwnd,IDC_SCROLL);
			SetScrollPos(hScrl,SB_CTL,tmp,TRUE);
			SendMessage(hwnd,WM_USER+1,(WPARAM)tmp,0);

			return TRUE;
		}
		break;
	case WM_USER+1:
		tmp=(int)wParam;
		hList=GetDlgItem(hwnd,IDC_MEMORY);
		if (SendMessage(hList,LB_GETTEXTLEN,0,0)!=LB_ERR)
		while(SendMessage(hList,LB_DELETESTRING,0,0));

		strcpy(buf,"          00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
		SendMessage(hList,LB_INSERTSTRING,0,(LPARAM)buf);

		byte bank_bkup;
		bank_bkup=(g_gb[0]->get_cpu()->get_ram_bank()-g_gb[0]->get_cpu()->get_ram())/0x1000;

		for (i=0;i<10;i++){
			g_gb[0]->get_cpu()->set_ram_bank(tmp>>8);
			sprintf(buf,"%d:[%04X]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",tmp>>8,((tmp&0xff)<<4)+0xd000,
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd000),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd001),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd002),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd003),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd004),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd005),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd006),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd007),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd008),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd009),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd010),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd011),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd012),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd013),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd014),
				g_gb[0]->get_cpu()->read(((tmp&0xff)<<4)+0xd015));
				SendMessage(hList,LB_INSERTSTRING,1+i,(LPARAM)buf);
			tmp++;
		}
		g_gb[0]->get_cpu()->set_ram_bank(bank_bkup);
		break;
	case WM_TIMER:
		hScrl=GetDlgItem(hwnd,IDC_SCROLL);
		scr.cbSize=sizeof(scr);
		scr.fMask=SIF_POS;
		GetScrollInfo(hScrl,SB_CTL,&scr);
		SendMessage(hwnd,WM_USER+1,(WPARAM)scr.nPos,0);
		break;
	case WM_CLOSE:
		KillTimer(hwnd,789);
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static char tmp_code[256],tmp_name[256];

static BOOL CALLBACK CheatInputProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hwnd,IDC_CODE,tmp_code);
		SetDlgItemText(hwnd,IDC_NAME,tmp_name);
		return TRUE;
	
	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			GetDlgItemText(hwnd,IDC_CODE,tmp_code,256);
			GetDlgItemText(hwnd,IDC_NAME,tmp_name,256);
			DestroyWindow(hwnd);
			return TRUE;
		}
		if(LOWORD(wParam)==IDCANCEL){
			DestroyWindow(hwnd);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static void add_cheat_list(HWND list,char *code,char *name,bool add=true,bool enable=true)
{
	LVITEM lvi;
	int place;
	char buf[256],*p;

	if (add){

		int i;
		int len;
		strcpy(buf,code);
		len=strlen(buf);

		p=buf;
		cheat_dat tmp_dat;
		cheat_dat *tmp=&tmp_dat;

		for (i=0;i<len;i++){//�啶����
			if (islower(buf[i])){
				buf[i]=toupper(buf[i]);
			}
		}

		char *cpy=buf;

		while(len>=8){
			tmp->code=((cpy[0]>='A'&&cpy[0]<='F')?cpy[0]-'A'+10:cpy[0]-'0')<<4|
				((cpy[1]>='A'&&cpy[1]<='F')?cpy[1]-'A'+10:cpy[1]-'0');
			tmp->dat=((cpy[2]>='A'&&cpy[2]<='F')?cpy[2]-'A'+10:cpy[2]-'0')<<4|
				((cpy[3]>='A'&&cpy[3]<='F')?cpy[3]-'A'+10:cpy[3]-'0');
			tmp->adr=((cpy[4]>='A'&&cpy[4]<='F')?cpy[4]-'A'+10:cpy[4]-'0')<<4|
				((cpy[5]>='A'&&cpy[5]<='F')?cpy[5]-'A'+10:cpy[5]-'0')|
				((cpy[6]>='A'&&cpy[6]<='F')?cpy[6]-'A'+10:cpy[6]-'0')<<12|
				((cpy[7]>='A'&&cpy[7]<='F')?cpy[7]-'A'+10:cpy[7]-'0')<<8;
			len-=9;
			if (len>=8){
				tmp->next=new cheat_dat;
				tmp=tmp->next;
				cpy+=9;
			}
		};
		tmp->next=NULL;

		if (name[0]=='\0')
			g_gb[0]->get_cheat()->create_unique_name(name);
		strcpy(tmp_dat.name,name);
		tmp_dat.enable=true;
		g_gb[0]->get_cheat()->add_cheat(&tmp_dat);

	}

	lvi.iItem=ListView_GetItemCount(list);
	lvi.pszText=buf;
	lvi.mask=LVIF_TEXT;
	lvi.iSubItem=0;
	strcpy(buf,enable?" ��":" �~");
	place=ListView_InsertItem(list,&lvi);

	ListView_SetItemText(list,place,1,code);
	ListView_SetItemText(list,place,2,name);
}

static void struct_list(HWND hlist)
{
	ListView_DeleteAllItems(hlist);

	std::list<cheat_dat>::iterator ite;
	cheat_dat *cur;
	for (ite=g_gb[0]->get_cheat()->get_first();ite!=g_gb[0]->get_cheat()->get_end();ite++){
		char tmp[256];
		tmp[0]=0;
		cur=&(*ite);
		do{
			sprintf(tmp,"%s:%02X%02X%02X%02X",tmp,cur->code,cur->dat,cur->adr&0xFF,(cur->adr>>8));
			cur=cur->next;
		}while(cur);
		add_cheat_list(hlist,tmp+1,ite->name,false,ite->enable);
	}
}

static BOOL CALLBACK CheatProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static HWND hlist;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		hlist=GetDlgItem(hwnd,IDC_LIST);

		LVCOLUMN lvc;
		lvc.mask=LVCF_TEXT|LVCF_ORDER|LVCF_WIDTH|LVCF_FMT;
		lvc.fmt=LVCFMT_LEFT;
		lvc.iOrder=0;
		lvc.pszText="���O";
		lvc.cx=180;
		ListView_InsertColumn(hlist,0,&lvc);
		lvc.pszText="�R�[�h";
		lvc.cx=80;
		ListView_InsertColumn(hlist,0,&lvc);
		lvc.pszText="�L��";
		lvc.cx=40;
		ListView_InsertColumn(hlist,0,&lvc);

		struct_list(hlist);

		return TRUE;

	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_ADD){
			tmp_code[0]='\0';
			tmp_name[0]='\0';
			if (DialogBox(hInstance,MAKEINTRESOURCE(IDD_CHEAT_INPUT),hwnd,CheatInputProc)==IDOK){
				add_cheat_list(hlist,tmp_code,tmp_name);
			}
		}
		else if (LOWORD(wParam)==IDC_DELETE){
			int sel=ListView_GetSelectionMark(hlist);
			if (sel!=-1){
				char buf[256];
				ListView_GetItemText(hlist,sel,2,buf,256);
				g_gb[0]->get_cheat()->delete_cheat(buf);
				ListView_DeleteItem(hlist,sel);
			}
		}
		else if (LOWORD(wParam)==ID_ALL_DELETE){
			if (MessageBox(hwnd,"��낵���ł����H","TGB Cheat Code",MB_OKCANCEL)==IDOK){
				ListView_DeleteAllItems(hlist);
				g_gb[0]->get_cheat()->clear();
			}
		}
		else if (LOWORD(wParam)==ID_CHEAT_SAVE){
			char dir[256],save_dir[256],filename[256]="";
			GetCurrentDirectory(256,dir);
			config->get_media_dir(save_dir);
			SetCurrentDirectory(save_dir);
			OPENFILENAME ofn;
			ZeroMemory(&ofn,sizeof(ofn));
			ofn.hInstance=hInstance;
			ofn.hwndOwner=hWnd;
			ofn.lStructSize=sizeof(ofn);
			ofn.lpstrDefExt="tch";
			ofn.lpstrFilter="TGB cheat code file\0*.tch\0All Files (*.*)\0*.*\0\0";
			ofn.nMaxFile=256;
			ofn.nMaxFileTitle=256;
			ofn.lpstrFileTitle=filename;
			ofn.lpstrTitle="GB Cheat Save";
			ofn.lpstrInitialDir=save_dir;
			if (GetSaveFileName(&ofn)==IDOK){
				if (strstr(filename,".tch")==NULL) strcat(filename,".tch");
				FILE *tmp_file=fopen(filename,"w");
				g_gb[0]->get_cheat()->save(tmp_file);
				fclose(tmp_file);
			}
			SetCurrentDirectory(dir);
		}
		else if (LOWORD(wParam)==ID_CHEAT_LOAD){
			char dir[256],save_dir[256],filename[256];
			GetCurrentDirectory(256,dir);
			config->get_media_dir(save_dir);
			SetCurrentDirectory(save_dir);
			OPENFILENAME ofn;
			ZeroMemory(&ofn,sizeof(ofn));
			ofn.hInstance=hInstance;
			ofn.hwndOwner=hWnd;
			ofn.lStructSize=sizeof(ofn);
			ofn.lpstrDefExt="tch";
			ofn.lpstrFilter="TGB cheat code file\0*.tch\0All Files (*.*)\0*.*\0\0";
			ofn.nMaxFile=256;
			ofn.nMaxFileTitle=256;
			ofn.lpstrFileTitle=filename;
			ofn.lpstrTitle="GB Cheat Load";
			ofn.lpstrInitialDir=save_dir;
			if (GetOpenFileName(&ofn)==IDOK){
				FILE *tmp_file=fopen(filename,"r");
				g_gb[0]->get_cheat()->load(tmp_file);
				fclose(tmp_file);
			}
			SetCurrentDirectory(dir);

			struct_list(hlist);
		}

		break;

	case WM_NOTIFY:
		if (wParam==IDC_LIST){
			NMHDR *pnm=(LPNMHDR)lParam;
			if (pnm->code==NM_DBLCLK){
				int sel=ListView_GetSelectionMark(hlist);
				if (sel!=-1){
					char buf[256];
					ListView_GetItemText(hlist,sel,2,buf,256);
					std::list<cheat_dat>::iterator ite=g_gb[0]->get_cheat()->find_cheat(buf);
					ite->enable=!ite->enable;
					ListView_SetItemText(hlist,sel,0,ite->enable?" ��":" �~");
				}
			}
			else if (pnm->code==NM_RCLICK){
				int sel=ListView_GetSelectionMark(hlist);
				if (sel!=-1){
					char buf[256];
					ListView_GetItemText(hlist,sel,1,buf,256);
					strcpy(tmp_code,buf);
					ListView_GetItemText(hlist,sel,2,buf,256);
					strcpy(tmp_name,buf);
					if (DialogBox(hInstance,MAKEINTRESOURCE(IDD_CHEAT_INPUT),hwnd,CheatInputProc)==IDOK){
						ListView_DeleteItem(hlist,sel);						
						g_gb[0]->get_cheat()->delete_cheat(buf);
						add_cheat_list(hlist,tmp_code,tmp_name);
					}
				}
			}
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

static BOOL CALLBACK ParProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static char filename[256]="";

	HWND hList=GetDlgItem(hwnd,IDC_LIST);
	cheat_dat tmp_dat,*cur;
	std::list<cheat_dat>::iterator ite;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		filename[0]='\0';

		for (ite=g_gb[0]->get_cheat()->get_first();ite!=g_gb[0]->get_cheat()->get_end();ite++){
			char buf[256],buf2[256];
			buf[0]=0;
			cur=&(*ite);
			do{
				sprintf(buf,"%s%02X%02X%02X%02X",buf,cur->code,cur->dat,cur->adr&0xFF,(cur->adr>>8));
				cur=cur->next;
			}while(cur);
			sprintf(buf2,"%s:%s",buf,ite->name);
			SendMessage(hList,LB_ADDSTRING,0,(LPARAM)buf2);
		}
		CheckDlgButton(hwnd,IDC_SRAM,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_RAM1,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_RAM2,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_HIMEM,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_DEC,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_BYTE,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_NORMAL,BST_CHECKED);
		CheckDlgButton(hwnd,IDC_FOREVER,BST_CHECKED);
		return TRUE;
	
	case WM_COMMAND:
		if ((LOWORD(wParam)==IDC_DELETE)||((HIWORD(wParam)==LBN_DBLCLK)&&(LOWORD(wParam)==IDC_LIST))){
			int sel=-1;
			char buf[64],*p,*pp;
			if (SendMessage(hList,LB_GETTEXTLEN,0,0)==LB_ERR)
				return TRUE;
			while(!SendMessage(hList,LB_GETSEL,++sel,0));
			if (SendMessage(hList,LB_GETTEXT,sel,(LPARAM)buf)!=LB_ERR){
				p=buf;
				pp=p-1;
				while(p=strstr(pp+1,":")) pp=p;
				g_gb[0]->get_cheat()->delete_cheat(pp+1);
				SendMessage(hList,LB_DELETESTRING,sel,0);
			}
			return TRUE;
		}
		else if (HIWORD(wParam)==LBN_DBLCLK){
			if (LOWORD(wParam)==IDC_DEST){
				int sel=-1;
				char buf[256];
				while(!SendMessage((HWND)lParam,LB_GETSEL,++sel,0));
				SendMessage((HWND)lParam,LB_GETTEXT,sel,(LPARAM)buf);
				SetDlgItemText(hwnd,IDC_ADDR,buf+1);
				return FALSE;
			}
		}
		else if(LOWORD(wParam)==IDOK){
			DestroyWindow(hwnd);
			return TRUE;
		}
		else if(LOWORD(wParam)==IDC_ALL_CLEAR){
			g_gb[0]->get_cheat()->clear();
			if (SendMessage(hList,LB_GETTEXTLEN,0,0)!=LB_ERR)
			while(SendMessage(hList,LB_DELETESTRING,0,0));
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_ADD){
			int i;
			char buf[256],*p;
			int len;
			GetDlgItemText(hwnd,IDC_CODE,buf,256);
			len=strlen(buf);

			p=buf;
			cheat_dat *tmp=&tmp_dat;

			for (i=0;i<len;i++){//�啶����
				if (islower(buf[i])){
					buf[i]=toupper(buf[i]);
				}
			}

			char *cpy=buf;

			while(len>=8){
				tmp->code=((cpy[0]>='A'&&cpy[0]<='F')?cpy[0]-'A'+10:cpy[0]-'0')<<4|
					((cpy[1]>='A'&&cpy[1]<='F')?cpy[1]-'A'+10:cpy[1]-'0');
				tmp->dat=((cpy[2]>='A'&&cpy[2]<='F')?cpy[2]-'A'+10:cpy[2]-'0')<<4|
					((cpy[3]>='A'&&cpy[3]<='F')?cpy[3]-'A'+10:cpy[3]-'0');
				tmp->adr=((cpy[4]>='A'&&cpy[4]<='F')?cpy[4]-'A'+10:cpy[4]-'0')<<4|
					((cpy[5]>='A'&&cpy[5]<='F')?cpy[5]-'A'+10:cpy[5]-'0')|
					((cpy[6]>='A'&&cpy[6]<='F')?cpy[6]-'A'+10:cpy[6]-'0')<<12|
					((cpy[7]>='A'&&cpy[7]<='F')?cpy[7]-'A'+10:cpy[7]-'0')<<8;
				len-=9;
				if (len>=8){
					tmp->next=new cheat_dat;
					tmp=tmp->next;
					cpy+=9;
				}
			};
			tmp->next=NULL;

			char name[16];

			GetDlgItemText(hwnd,IDC_ADD_NAME,name,16);
			if (name[0]=='\0')
				g_gb[0]->get_cheat()->create_unique_name(name);
			strcpy(tmp_dat.name,name);
			tmp_dat.enable=true;
			g_gb[0]->get_cheat()->add_cheat(&tmp_dat);

			if (SendMessage(hList,LB_GETTEXTLEN,0,0)!=LB_ERR)
			while(SendMessage(hList,LB_DELETESTRING,0,0));

			for (ite=g_gb[0]->get_cheat()->get_first();ite!=g_gb[0]->get_cheat()->get_end();ite++){
				char buf[256],buf2[256];
				buf[0]=0;
				cur=&(*ite);
				do{
					sprintf(buf,"%s%02X%02X%02X%02X:",buf,cur->code,cur->dat,cur->adr&0xFF,(cur->adr>>8));
					cur=cur->next;
				}while(cur);
				sprintf(buf2,"%s%s",buf,ite->name);
				SendMessage(hList,LB_ADDSTRING,0,(LPARAM)buf2);
			}
		}
		else if (LOWORD(wParam)==IDC_SEARCH){
			int i;
			DWORD dat;
			char buf[256];
			char str[16];
			int before_count=0;
			bool normal;
			std::list<int> bef_addrs;
			std::list<int>::iterator ite;

			int dat_len,now_count;

			dat_len=(IsDlgButtonChecked(hwnd,IDC_BYTE)==BST_CHECKED)?1:0+
				(IsDlgButtonChecked(hwnd,IDC_WORD)==BST_CHECKED)?2:0+
				(IsDlgButtonChecked(hwnd,IDC_FWORD)==BST_CHECKED)?3:0+
				(IsDlgButtonChecked(hwnd,IDC_DWORD)==BST_CHECKED)?4:0;

			GetDlgItemText(hwnd,IDC_SEARCH_NUM,buf,256);
			dat=strtoul(buf,NULL,(IsDlgButtonChecked(hwnd,IDC_HEX)==BST_CHECKED)?16:10);
			normal=(IsDlgButtonChecked(hwnd,IDC_NORMAL)==BST_CHECKED)?true:false;

			if (SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_GETTEXTLEN,0,0)!=LB_ERR){
				while(true){
					if (SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_GETTEXT,0,(LPARAM)buf)==LB_ERR)
						break;
					int cash_adr;
					if (buf[2]==':')
						cash_adr=((buf[1]-'0')<<16)|(strtoul(buf+3,NULL,16)&0xFFFF);
					else
						cash_adr=(WORD)(strtoul(buf+1,NULL,16)&0xFFFF);
					bef_addrs.push_back(cash_adr);
					SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_DELETESTRING,0,0);
				}
			}

			now_count=0;

			if (IsDlgButtonChecked(hwnd,IDC_NORAM)==BST_CHECKED){
				for (i=0x1000;i<0x7FFF-dat_len;i++){
					if (now_count!=0){
						if (((dat>>(8*now_count))&0xff)==g_gb[0]->get_cpu()->get_ram()[i])
							now_count++;
						else{
							i-=now_count;
							now_count=0;
						}
					}
					else if ((dat&0xff)==g_gb[0]->get_cpu()->get_ram()[i])
						now_count++;

					if (dat_len==now_count){
						if (normal){
							sprintf(str,"$%d:%04X",(i-now_count+1)>>12,((i-now_count+1)&0xfff)+0xd000);
							SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_ADDSTRING,0,(LPARAM)str);
						}
						else{
							for (ite=bef_addrs.begin();ite!=bef_addrs.end();ite++){
								if (*ite==(((i-now_count+1)&0x0fff)|(((i-now_count+1)&0x7000)<<4)|0xd000)){
									sprintf(str,"$%d:%04X",(i-now_count+1)>>12,((i-now_count+1)&0xfff)+0xd000);
									SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_ADDSTRING,0,(LPARAM)str);
								}
							}
						}
					}
				}
				now_count=0;
			}

			for (i=0;i<0xFFFF-dat_len;i++){
				if ((i<0xA000)||((i>=0xA000&&i<0xC000)&&((IsDlgButtonChecked(hwnd,IDC_SRAM)!=BST_CHECKED)))||
					((i>=0xC000&&i<0xD000)&&((IsDlgButtonChecked(hwnd,IDC_RAM1)!=BST_CHECKED)))||
					((i>=0xD000&&i<0xE000)&&((IsDlgButtonChecked(hwnd,IDC_RAM2)!=BST_CHECKED)))||
					(i>=0xE000&&i<0xFF80)||((i>=0xFF80)&&(IsDlgButtonChecked(hwnd,IDC_HIMEM)!=BST_CHECKED))){
					continue;
				}

				if (now_count!=0){
					if (((dat>>(8*now_count))&0xff)==g_gb[0]->get_cpu()->read(i))
						now_count++;
					else{
						i-=now_count;
						now_count=0;
					}
				}
				else if ((dat&0xff)==g_gb[0]->get_cpu()->read(i))
						now_count++;

				if (dat_len==now_count){
					if (normal){
						sprintf(str,"$%04X",i-now_count+1);
						SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_ADDSTRING,0,(LPARAM)str);
					}
					else{
						for (ite=bef_addrs.begin();ite!=bef_addrs.end();ite++){
							if (*ite==(i-now_count+1)){
								sprintf(str,"$%04X",i-now_count+1);
								SendMessage(GetDlgItem(hwnd,IDC_DEST),LB_ADDSTRING,0,(LPARAM)str);
							}
						}
					}
				}
			}
			CheckDlgButton(hwnd,IDC_MUCH,BST_CHECKED);
			CheckDlgButton(hwnd,IDC_NORMAL,BST_UNCHECKED);
			bef_addrs.clear();
		}
		else if (LOWORD(wParam)==IDC_ADD_SEARCH){
			char buf[256];
			char name[16];
			BYTE bank;
			WORD adr,adr2;
			DWORD dat,dat2;
			bool have_2=true;
			BYTE code;

			GetDlgItemText(hwnd,IDC_ADDR,buf,256);
			if (buf[1]==':'){
				bank=buf[0]-'0';
				buf[0]=buf[2];
				buf[1]=buf[3];
				buf[2]=buf[4];
				buf[3]=buf[5];
				buf[4]='\0';
			}
			else
				bank=0;
			adr=(WORD)strtoul(buf,NULL,16);
			GetDlgItemText(hwnd,IDC_DAT,buf,256);
			dat=strtoul(buf,NULL,(IsDlgButtonChecked(hwnd,IDC_HEX)==BST_CHECKED)?16:10);
			GetDlgItemText(hwnd,IDC_ADDR2,buf,256);
			adr2=(WORD)strtoul(buf,NULL,16);
			if (buf[0]=='\0')
				have_2=false;
			GetDlgItemText(hwnd,IDC_DAT2,buf,256);
			dat2=strtoul(buf,NULL,(IsDlgButtonChecked(hwnd,IDC_HEX)==BST_CHECKED)?16:10);
			if (buf[0]=='\0')
				have_2=false;

			int dat_len,i;

			dat_len=(IsDlgButtonChecked(hwnd,IDC_BYTE)==BST_CHECKED)?1:0+
				(IsDlgButtonChecked(hwnd,IDC_WORD)==BST_CHECKED)?2:0+
				(IsDlgButtonChecked(hwnd,IDC_FWORD)==BST_CHECKED)?3:0+
				(IsDlgButtonChecked(hwnd,IDC_DWORD)==BST_CHECKED)?4:0;

			if (IsDlgButtonChecked(hwnd,IDC_ONCE)==BST_CHECKED){
				if (adr<0x8000)
					return TRUE;
				for (i=0;i<dat_len;i++)
					if (bank){
						g_gb[0]->get_cpu()->get_ram()[(bank*0x1000+adr-0xd000+i)&0x7fff]=(BYTE)((dat>>(8*i))&0xFF);
					}
					else
						g_gb[0]->get_cpu()->write(adr+i,(BYTE)((dat>>(8*i))&0xFF));
				return TRUE;
			}
			else {
				if (IsDlgButtonChecked(hwnd,IDC_FOREVER)==BST_CHECKED){
					if (bank) code=0x90+bank;
					else code=1;
				}
				else if (IsDlgButtonChecked(hwnd,IDC_LOOP)==BST_CHECKED){
					code=0x10;
					dat_len=1;
				}
				else if (IsDlgButtonChecked(hwnd,IDC_CMP_EQUAL)==BST_CHECKED){
					code=0x20;
					dat_len=1;
				}
				else if (IsDlgButtonChecked(hwnd,IDC_CMP_LESS)==BST_CHECKED){
					code=0x21;
					dat_len=1;
				}
				else if (IsDlgButtonChecked(hwnd,IDC_CMP_LARGE)==BST_CHECKED){
					code=0x22;
					dat_len=1;
				}
				tmp_dat.code=code;

				for (i=0;i<dat_len;i++){
					GetDlgItemText(hwnd,IDC_CHEAT_NAME,name,16);
					if (name[0]=='\0')
						g_gb[0]->get_cheat()->create_unique_name(name);
					else if (dat_len>1)
						sprintf(name,"%s%d",name,i);
					strcpy(tmp_dat.name,name);

					if (have_2){
						if (tmp_dat.code==10){
							tmp_dat.adr=adr;
							tmp_dat.dat=(byte)(dat&0xff);
							tmp_dat.next=new cheat_dat;
							tmp_dat.next->code=1;
							tmp_dat.next->adr=adr2+i;
							tmp_dat.next->dat=(byte)((dat2>>(8*i))&0xff);
							tmp_dat.next->next=NULL;
						}
						else{
							tmp_dat.adr=adr+i;
							tmp_dat.dat=(byte)((dat>>(8*i))&0xff);
							tmp_dat.next=new cheat_dat;
							tmp_dat.next->code=1;
							tmp_dat.next->adr=adr2;
							tmp_dat.next->dat=(byte)(dat2&0xff);
							tmp_dat.next->next=NULL;
						}
					}
					else{
						tmp_dat.adr=adr+i;
						tmp_dat.dat=(byte)((dat>>(8*i))&0xff);
						tmp_dat.next=NULL;
					}
					tmp_dat.enable=true;
					g_gb[0]->get_cheat()->add_cheat(&tmp_dat);
				}


				if (SendMessage(hList,LB_GETTEXTLEN,0,0)!=LB_ERR)
				while(SendMessage(hList,LB_DELETESTRING,0,0));

				for (ite=g_gb[0]->get_cheat()->get_first();ite!=g_gb[0]->get_cheat()->get_end();ite++){
					char buf[256],buf2[256];
					buf[0]=0;
					cur=&(*ite);
					do{
						sprintf(buf,"%s%02X%02X%02X%02X",buf,cur->code,cur->dat,cur->adr&0xFF,(cur->adr>>8));
						cur=cur->next;
					}while(cur);
					sprintf(buf2,"%s:%s",buf,ite->name);
					SendMessage(hList,LB_ADDSTRING,0,(LPARAM)buf2);
				}
			}
		}
		else if ((LOWORD(wParam)==IDC_OVER_RIDE)&&(filename[0]!='\0')){
			FILE *tmp_file=fopen(filename,"w");
			g_gb[0]->get_cheat()->save(tmp_file);
			fclose(tmp_file);
		}
		else if ((LOWORD(wParam)==IDC_OVER_RIDE)||(LOWORD(wParam)==IDC_CHEAT_SAVE)){
			char dir[256],save_dir[256];
			GetCurrentDirectory(256,dir);
			config->get_media_dir(save_dir);
			SetCurrentDirectory(save_dir);
			OPENFILENAME ofn;
			ZeroMemory(&ofn,sizeof(ofn));
			ofn.hInstance=hInstance;
			ofn.hwndOwner=hWnd;
			ofn.lStructSize=sizeof(ofn);
			ofn.lpstrDefExt="tch";
			ofn.lpstrFilter="TGB cheat code file\0*.tch\0All Files (*.*)\0*.*\0\0";
			ofn.nMaxFile=256;
			ofn.nMaxFileTitle=256;
			ofn.lpstrFileTitle=filename;
			ofn.lpstrTitle="TGB Cheat Save";
			ofn.lpstrInitialDir=save_dir;
			if (GetSaveFileName(&ofn)==IDOK){
				if (strstr(filename,".tch")==0) strcat(filename,".tch");
				FILE *tmp_file=fopen(filename,"w");
				g_gb[0]->get_cheat()->save(tmp_file);
				fclose(tmp_file);
			}
			SetCurrentDirectory(dir);
		}
		else if (LOWORD(wParam)==IDC_CHEAT_READ){
			char dir[256],save_dir[256];
			GetCurrentDirectory(256,dir);
			config->get_media_dir(save_dir);
			SetCurrentDirectory(save_dir);
			OPENFILENAME ofn;
			ZeroMemory(&ofn,sizeof(ofn));
			ofn.hInstance=hInstance;
			ofn.hwndOwner=hWnd;
			ofn.lStructSize=sizeof(ofn);
			ofn.lpstrDefExt="tch";
			ofn.lpstrFilter="TGB cheat code file\0*.tch\0All Files (*.*)\0*.*\0\0";
			ofn.nMaxFile=256;
			ofn.nMaxFileTitle=256;
			ofn.lpstrFileTitle=filename;
			ofn.lpstrTitle="TGB Cheat Load";
			ofn.lpstrInitialDir=save_dir;
			if (GetOpenFileName(&ofn)==IDOK){
				FILE *tmp_file=fopen(filename,"r");
				g_gb[0]->get_cheat()->load(tmp_file);
				fclose(tmp_file);
			}

			if (SendMessage(hList,LB_GETTEXTLEN,0,0)!=LB_ERR)
			while(SendMessage(hList,LB_DELETESTRING,0,0));

			for (ite=g_gb[0]->get_cheat()->get_first();ite!=g_gb[0]->get_cheat()->get_end();ite++){
				char buf[256],buf2[256];
				buf[0]=0;
				cur=&(*ite);
				do{
					sprintf(buf,"%s%02X%02X%02X%02X",buf,cur->code,cur->dat,cur->adr&0xFF,(cur->adr>>8));
					cur=cur->next;
				}while(cur);
				sprintf(buf2,"%s:%s",buf,ite->name);
				SendMessage(hList,LB_ADDSTRING,0,(LPARAM)buf2);
			}
			SetCurrentDirectory(dir);
		}

		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	}

    return FALSE;
}

// �ʐM���[�`��

/*
static bool b_terminal=false;
*/
static bool b_server;

static BOOL CALLBACK ConnectProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static char my_rom[256]="";
	static char tar_rom[256]="use same rom image";

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			list<string> ips=winsock_obj::get_instance()->get_ipaddrs();
			for (list<string>::iterator it=ips.begin();it!=ips.end();it++)
				SendDlgItemMessage(hwnd,IDC_YOUR_IP,LB_ADDSTRING,(WPARAM)0,(LPARAM)it->c_str());

			CheckDlgButton(hwnd,IDC_SERVER,BST_CHECKED);
			EnableWindow(GetDlgItem(hwnd,IDC_IPADDR),FALSE);

			SetDlgItemText(hwnd,IDC_STATUS,"Not connected");
			EnableWindow(GetDlgItem(hwnd,IDC_START),TRUE);
			EnableWindow(GetDlgItem(hwnd,IDC_TERMINATE),FALSE);

			SetDlgItemText(hwnd,IDC_MY_ROM,my_rom);
			SetDlgItemText(hwnd,IDC_TAR_ROM,tar_rom);

			SendDlgItemMessage(hwnd,IDC_IPADDR,CB_RESETCONTENT,0,0);

			for (int i=0;i<4;i++)
				if (config->ip_addrs[i][0])
					SendDlgItemMessage(hwnd,IDC_IPADDR,CB_ADDSTRING,0,(LPARAM)config->ip_addrs[i]);
		}
		return TRUE;
	
	case WM_COMMAND:
		if(LOWORD(wParam)==IDCANCEL){
			// �ڑ��������ɉ����ꂽ��ڑ����L�����Z������
			DestroyWindow(hwnd);
			trans_hwnd=NULL;
			return TRUE;
		}
		else if (LOWORD(wParam)==IDC_TERMINATE){
			SetDlgItemText(hwnd,IDC_STATUS,"Not connected");
			EnableWindow(GetDlgItem(hwnd,IDC_START),TRUE);
			EnableWindow(GetDlgItem(hwnd,IDC_TERMINATE),FALSE);
		}
		else if (LOWORD(wParam)==IDC_SERVER)
			EnableWindow(GetDlgItem(hwnd,IDC_IPADDR),FALSE);
		else if (LOWORD(wParam)==IDC_CLIENT)
			EnableWindow(GetDlgItem(hwnd,IDC_IPADDR),TRUE);
		else if ((LOWORD(wParam)==IDC_ROM_SELECT1)||(LOWORD(wParam)==IDC_ROM_SELECT2)){
			char buf[256],buf2[256],dir[256];
			GetCurrentDirectory(256,dir);
			OPENFILENAME ofn;
			ZeroMemory(&ofn,sizeof(ofn));
			ofn.hInstance=hInstance;
			ofn.hwndOwner=hWnd;
			ofn.lStructSize=sizeof(ofn);
			ofn.lpstrDefExt="gb";
			ofn.lpstrFilter="Game Boy Rom Image (include archive file) (*.gb;*.gbc;*.cab;*.zip;*.rar;*.lzh)\0*.gb;*.gbc;*.cab;*.rar;*.zip;*.lzh\0All Files (*.*)\0*.*\0\0";
			ofn.nMaxFile=256;
			ofn.nMaxFileTitle=256;
			ofn.lpstrFile=buf;
			ofn.lpstrFileTitle=buf2;
			ofn.lpstrTitle="GB Rom Load";
			ofn.lpstrInitialDir=dir;
			buf[0]=buf2[0]='\0';
			if (GetOpenFileName(&ofn)==IDOK)
				SetDlgItemText(hwnd,(LOWORD(wParam)==IDC_ROM_SELECT1)?IDC_MY_ROM:IDC_TAR_ROM,buf);
		}
		else if (LOWORD(wParam)==IDC_START){
			b_server=(IsDlgButtonChecked(hwnd,IDC_SERVER)==BST_CHECKED);
			int port;
			{
				char tmp[256];
				GetDlgItemText(hwnd,IDC_PORT,tmp,sizeof(tmp));
				port=atoi(tmp);
				if (port<1024||port>65535){
					SetDlgItemText(hwnd,IDC_STATUS,"Invalid Port");
					return TRUE;
				}
			}
			char target[256];
			SendDlgItemMessage(hwnd,IDC_IPADDR,WM_GETTEXT,256,(LPARAM)target);
			if (!b_server&&target[0]=='\0'){
				SetDlgItemText(hwnd,IDC_STATUS,"Enter IP Address");
				return TRUE;
			}

			char tar_tmp[256];
			GetDlgItemText(hwnd,IDC_MY_ROM,my_rom,256);
			GetDlgItemText(hwnd,IDC_TAR_ROM,tar_tmp,256);
			strcpy(tar_rom,strcmp(tar_tmp,"use same rom image")==0?my_rom:tar_tmp);

			dmy_render=new dmy_renderer;
			render[0]->disable_check_pad();

			g_gb[0]=new gb(render[0],true,true);
			g_gb[1]=new gb(dmy_render,false,false);

			if (!load_rom_only(my_rom,0)||!load_rom_only(tar_rom,1)){
				delete g_gb[0];g_gb[0]=NULL;
				delete g_gb[1];g_gb[1]=NULL;
				SetDlgItemText(hwnd,IDC_STATUS,"Can't read the ROM");
				return TRUE;
			}

			if (b_server){
				SetDlgItemText(hwnd,IDC_STATUS,"Waiting for connection");

				// �Ƃ肠�����T�[�o���N��
				net=new tgb_netplay(port);
				net->send_sram((char*)g_gb[0]->get_rom()->get_sram(),
					g_gb[0]->get_rom()->get_sram_size());
			}
			else{
				// ip�A�h���X�����̍X�V
				char (*p)[20]=config->ip_addrs;
				for (int i = 0; i < 4; i++)
				{
					if (strcmp(target, p[i]) == 0) break;
					for (int j = (i == 4 ? 3 : i); j > 0; j--)
						strcpy(p[j], p[j - 1]);
				}
				strcpy(p[0],target);

				SetDlgItemText(hwnd,IDC_STATUS,"Already connected");
				// �N���C�A���g�N��
				net=new tgb_netplay(string(target),port);
				net->send_sram((char*)g_gb[0]->get_rom()->get_sram(),
					g_gb[0]->get_rom()->get_sram_size());
			}

			g_gb[0]->set_target(g_gb[1]);
			g_gb[1]->set_target(g_gb[0]);

			g_gb[0]->reset();
			g_gb[1]->reset();

			cur_mode=NETWORK_PREPARING;
			sended=0;

			EnableWindow(GetDlgItem(hwnd,IDC_START),FALSE);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;

	case WM_DESTROY:
		if (cur_mode==NETWORK_PREPARING){
			delete net;
			net=NULL;
			free_rom(0);
			free_rom(1);
			cur_mode=UNLOADED;
		}
		trans_hwnd=NULL;
		return TRUE;
	}

    return FALSE;
}

void send_chat_message(string &str)
{
	if (chat_hwnd)
		SendMessage(chat_hwnd,WM_OUTLOG,0,(LPARAM)(string(">> ")+str).c_str());
	else{
		char *p=new char[str.length()+1];
		strcpy(p,str.c_str());
		chat_list.push_back(p);
	}
}

static BOOL CALLBACK ChatProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	std::list<char*>::iterator ite;
	HWND hRich;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			CHARFORMAT frm={sizeof(frm)};
			hRich=GetDlgItem(hwnd,IDC_DISPLAY);
			SendMessage( hRich, EM_SETREADONLY, TRUE, 0);
			SendMessage( hRich, EM_EXLIMITTEXT, 0, 655350);
//			SendMessage( hRich, EM_SETBKGNDCOLOR, 0, 0x00d7d7bb);
			SendMessage( hRich, EM_SETBKGNDCOLOR, 0, 0x00664422);
			frm.dwMask=CFM_COLOR;
			frm.crTextColor=RGB(200,200,240);
			SendMessage( hRich, EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&frm);
			SendMessage( hRich, EM_SETMARGINS, EC_USEFONTINFO, 0);
		}
		chat_list.clear();
		chat_hwnd=NULL;
		send_chat_message(string("TGB mini Chat\n\n"));
		for (ite=chat_list.begin();ite!=chat_list.end();ite++){
			DWORD TextLength = SendMessage( hRich, WM_GETTEXTLENGTH, 0, 0);
			SendMessage( hRich, EM_SETSEL, (WPARAM)TextLength, (LPARAM) TextLength);
			SendMessage( hRich, EM_REPLACESEL, FALSE, (LPARAM)*ite);
			SendMessage( hRich, EM_SCROLLCARET, 0, 0);
		}
		chat_hwnd=hwnd;
		// ���g�����C���E�C���h�E�̉��Ɉړ�������
		RECT rect;
		GetWindowRect(hWnd,&rect);
		SetWindowPos(hwnd,NULL,rect.right,rect.top,0,0,SWP_NOSIZE|SWP_NOZORDER);
		return TRUE;

	case WM_COMMAND:
		if ((LOWORD(wParam)==IDC_MES)&&(HIWORD(wParam)==EN_CHANGE)&&((HWND)lParam==GetDlgItem(hwnd,IDC_MES))){
			char tmp[256],tmp2[256];
			GetDlgItemText(hwnd,IDC_MES,tmp,256);

			int len=strlen(tmp);
			if (tmp[len-1]=='\n'){
				SetDlgItemText(hwnd,IDC_MES,"");
				if (len>1){
					if (net) net->send_message(string(tmp));
					sprintf(tmp2,"<< %s",tmp);
					SendMessage(hwnd,WM_OUTLOG,NULL,(LPARAM)tmp2);
				}
			}
		}
		break;
	case WM_OUTLOG:
		char *p;
		p=new char[strlen((char*)lParam)+5];
		sprintf(p,">> %s",(char*)lParam);
		chat_list.push_back(p);

		hRich=GetDlgItem(hwnd,IDC_DISPLAY);
		DWORD TextLength;
		TextLength = SendMessage( hRich, WM_GETTEXTLENGTH, 0, 0);
		SendMessage( hRich, EM_SETSEL, (WPARAM)TextLength, (LPARAM) TextLength);
		SendMessage( hRich, EM_REPLACESEL, FALSE, (LPARAM)lParam);
		SendMessage( hRich, EM_SCROLLCARET, 0, 0);
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;
	case WM_DESTROY:
		chat_hwnd=NULL;
		for (ite=chat_list.begin();ite!=chat_list.end();ite++)
			delete [](char*)(*ite);
		chat_list.clear();
		return TRUE;
	}

    return FALSE;
}
