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

#include "../gb_core/renderer.h"

#pragma pack(push,gbr_procs_pack)
#pragma pack(4)

struct gbr_procs{
	void(*load)(unsigned char*,int); // �ǂݍ���
	void(*unload)(); // ��� (�Y���ƃ��������[�N���N����܂�)
	void(*run)(); // CPU�𑖂点��(456*154�N���b�N)
	void(*render)(short*,int); // �������ɔg�`���������݂܂�
	void(*select)(int); // �I�Ȃ��܂�
	void(*enable)(int,int); // �`�����l����On/Off��ύX���܂�
	void(*effect)(int,int); // �G�t�F�N�^��On/Off��ύX���܂�
};

#pragma pack(pop,gbr_procs_pack)

class gbr_sound : public sound_renderer
{
public:
	gbr_sound(gbr_procs *ref_procs);
	~gbr_sound();

	void render(short *buf,int samples);
	short *get_bef();

private:
	gbr_procs *procs;

	bool change;
	int rest,now;
	short bef_buf[160*2*10+2];
};

class gbr
{
public:
	gbr(renderer *ref,gbr_procs *ref_procs);
	~gbr();

	void run();
	void reset();
	void load_rom(byte *buf,int size);
	void select(int num) { cur_num=num; procs->select(num); }
	int get_num(void) { return cur_num; }
	void set_enable(int ch,int enable) { procs->enable(ch,enable); }
	void set_effect(int eff,int enable) { procs->effect(eff,enable); }

private:
	gbr_procs *procs;
	gbr_sound *gbr_snd;
	renderer *m_renderer;

	int cur_num;
	int frames;
	unsigned short vframe[160*144];
	short r,g;
};
