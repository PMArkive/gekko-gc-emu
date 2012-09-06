/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    bp_mem.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-03-10
 * @brief   Implementation of BP for the graphics processor
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "common.h"
#include "crc.h"
#include "memory.h"

#include <GL/glew.h>

#include "hw/hw_pe.h"

#include "renderer_gl3/shader_manager.h"

#include "video_core.h"
#include "vertex_manager.h"
#include "fifo.h"
#include "fifo_player.h"
#include "bp_mem.h"
#include "texture_decoder.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Graphics Processor namespace

#undef LOG_DEBUG
#define LOG_DEBUG(x,y, ...)

namespace gp {

BPMemory g_bp_regs; ///< BP memory/registers

f32						r_cc[4][4];
f32						r_kc[4][4];
void UploadColor(u8 addr, u32 value)
{
	if(addr & 1) // green/blue
	{
		if(!(value >> 23)) // color
		{
			// unpack
			r_cc[(addr - 0xe1) / 2][1] = ((value >> 12) & 0xff) / 255.0f;
			r_cc[(addr - 0xe1) / 2][2] = ((value >> 0) & 0xff) / 255.0f;



		}else{ // konstant
			// unpack
			r_kc[(addr - 0xe1) / 2][1] = ((value >> 12) & 0xff) / 255.0f;
			r_kc[(addr - 0xe1) / 2][2] = ((value >> 0) & 0xff) / 255.0f;
		}
	}else{ // red/alpha
		if(!(value >> 23)) // color
		{
			// unpack
			r_cc[(addr - 0xe0) / 2][3] = ((value >> 12) & 0xff) / 255.0f;
			r_cc[(addr - 0xe0) / 2][0] = ((value >> 0) & 0xff) / 255.0f;
		}else{ // konstant
			// unpack
			r_kc[(addr - 0xe0) / 2][3] = ((value >> 12) & 0xff) / 255.0f;
			r_kc[(addr - 0xe0) / 2][0] = ((value >> 0) & 0xff) / 255.0f;

			//set_modifed();
		}
	}

    glUniform4fv(glGetUniformLocation(shader_manager::g_shader_default_id, "bp_tev_color"), 4, (f32*)r_cc);
    glUniform4fv(glGetUniformLocation(shader_manager::g_shader_default_id, "bp_tev_konst"), 4, (f32*)r_kc);
}

/// Write a BP register
void BPRegisterWrite(u8 addr, u32 data) {
    LOG_DEBUG(TGP, "BP_LOAD [%02x] = %08x", addr, data);

    // Write to renderer
	//	Note: This should always happen first to compare what was last written!
    video_core::g_renderer->WriteBP(addr, data);

	// Write data to bp memory
    g_bp_regs.mem[addr] = data;

    // Adjust GX globals accordingly
    switch(addr) {
    case BP_REG_GENMODE: // GEN_MODE
        video_core::g_renderer->SetGenerationMode();
        break;

    case BP_REG_SCISSORTL: // SU_SCIS0 - Scissorbox Top Left Corner
    case BP_REG_SCISSORBR: // SU_SCIS1 - Scissorbox Bottom Right Corner
    case BP_REG_SCISSOROFFSET:
        video_core::g_renderer->SetScissorBox();
        break;

    case BP_REG_LINEPTWIDTH: // SU_LPSIZE - Line and Point Size
        //gx_states::set_lpsize();
        LOG_DEBUG(TGP, "BP-> SU_LPSIZE");
        break;

    case BP_REG_TREF + 0: // RAS_TREF0
    case BP_REG_TREF + 1: // RAS_TREF1
    case BP_REG_TREF + 2: // RAS_TREF2
    case BP_REG_TREF + 3: // RAS_TREF3
    case BP_REG_TREF + 4: // RAS_TREF4
    case BP_REG_TREF + 5: // RAS_TREF5
    case BP_REG_TREF + 6: // RAS_TREF6
    case BP_REG_TREF + 7: // RAS_TREF7
        //gx_tev::set_modifed();
        LOG_DEBUG(TGP, "BP-> RAS_TREFx");
        break;

    case BP_REG_PE_ZMODE: // PE_ZMODE set z mode
        video_core::g_renderer->SetDepthMode();
        break;

    case BP_REG_PE_CMODE0: // PE_CMODE0 dithering / blend mode/color_update/alpha_update/set_dither
        //gx_states::set_cmode0();
        if (data & 0xFFFF) {
            // Set LogicOp Blending Mode
            if (data & 2) {
                video_core::g_renderer->SetLogicOpMode();
            }
            // Set Dithering Mode
            if (data & 4) {
                video_core::g_renderer->SetDitherMode();
            }
            // Set Blending Mode
            if (data & 0xFE1) {
                video_core::g_renderer->SetBlendMode(false);
            }
            // Set Color Mask
            if (data & 0x18) {
                // TODO(ShizZy): Renable when we have EFB emulated
                video_core::g_renderer->SetColorMask();
            }
        }
        break;

    case BP_REG_PE_CMODE1: // PE_CMODE1 destination alpha
        //gx_tev::set_modifed();
        LOG_DEBUG(TGP, "BP-> PE_CMODE1");
        break;

    case BP_REG_PE_CONTROL: // PE_CONTROL comp z location z_comp_loc(0x43000040)pixel_fmt(0x43000041)
        //video_core::g_renderer->SetColorMask(); TODO(ShizZy): Renable when we have EFB emulated
        break;

    case BP_REG_PE_DRAWDONE: // PE_DONE - draw done

	    if (g_bp_regs.mem[0x45] & 0x2) { // enable interrupt
            // Flush vertex buffer
            vertex_manager::Flush();

            video_core::g_renderer->SwapBuffers();
            if (fifo_player::IsRecording())
                fifo_player::FrameFinished();
            FifoReset();
            GX_PE_FINISH = 1;
        }
        break;

    case BP_REG_PE_TOKEN: // PE_TOKEN
        GX_PE_TOKEN_VALUE = (data & 0xffff);
        LOG_DEBUG(TGP, "BP-> PE_TOKEN");
        break;

    case BP_REG_PE_TOKEN_INT: // PE_TOKEN_INT
        GX_PE_TOKEN_VALUE = (data & 0xffff); 
        GX_PE_TOKEN = 1;
        LOG_DEBUG(TGP, "BP-> PE_TOKEN_INT");
        break;

    case BP_REG_PE_CLEAR_AR: // PE copy clear AR - set clear alpha and red components
    case BP_REG_PE_CLEAR_GB: // PE copy clear GB - green and blue
        //gx_states::set_copyclearcolor();
        LOG_DEBUG(TGP, "BP-> PE_COPY_CLEAR_COLOR");
        break;

    case BP_REG_PE_CLEAR_Z: // PE copy clear Z - 24-bit Z value
        //gx_states::set_copyclearz();
	    // unpack z data
	    // send to efb
	    //glClearDepth(((GLclampd)BP_PE_COPYCLEAR_Z_VALUE) / GX_VIEWPORT_ZMAX);
        LOG_DEBUG(TGP, "BP-> PE_COPY_CLEAR_X");
        break;

    case BP_REG_PE_COPY_EXECUTE:  // pe copy execute
        {
            Rect rect;
            rect.x = gp::g_bp_regs.efb_top_left.x;
            rect.y = gp::g_bp_regs.efb_top_left.y;
            rect.width = gp::g_bp_regs.efb_height_width.x + 1;
            rect.height = gp::g_bp_regs.efb_height_width.y + 1;

            BPPECopyExecute pe_copy;
            pe_copy._u32 = data;

            if (pe_copy.copy_to_xfb) {
                f32 scale_y;
                if (pe_copy.scale_invert) {
                    scale_y = 256.0f / (f32)gp::g_bp_regs.disp_copy_y_scale;
                } else {
                    scale_y = (f32)gp::g_bp_regs.disp_copy_y_scale / 256.0f;
                }
            
                u32 xfb_height = (u32)(((f32)gp::g_bp_regs.efb_height_width.y + 1.0f) * scale_y);
                u32 xfb_width = gp::g_bp_regs.disp_stride << 4;

                video_core::g_renderer->CopyEFB(
                    RendererBase::kFramebuffer_VirtualXFB,
                    rect,
                    xfb_width,
                    xfb_height);
            } else {
                // TODO(ShizZy): Implement copy to texture
            }

            if (pe_copy.clear) {
                bool enable_color = gp::g_bp_regs.cmode0.color_update;
                bool enable_alpha = gp::g_bp_regs.cmode0.alpha_update;
                bool enable_z = gp::g_bp_regs.zmode.update_enable;

                // Forcibly disable alpha if the pixel format does not support it
                if (gp::g_bp_regs.zcontrol.pixel_format == BP_PIXELFORMAT_RGB8_Z24 || 
                    gp::g_bp_regs.zcontrol.pixel_format == BP_PIXELFORMAT_RGB565_Z16 || 
                    gp::g_bp_regs.zcontrol.pixel_format == BP_PIXELFORMAT_Z24) {
                    enable_alpha = false;
                }

                if (enable_color || enable_alpha || enable_z) {
                    u32 color = ((gp::g_bp_regs.clear_ar & 0xffff) << 16) | 
                        (gp::g_bp_regs.clear_gb & 0xffff);

                    u32 z = gp::g_bp_regs.clear_z & 0xffffff;

                    video_core::g_renderer->Clear(
                        rect,                               // Clear rectangle
                        enable_color,                       // Enable color clearing
                        enable_alpha,                       // Enable alpha clearing
                        enable_z,                           // Enable depth clearing
                        color,                              // Clear color
                        z);                                 // Clear depth
                }
        
            }
            break;
        }

    case BP_REG_LOADTLUT0: // TX_LOADTLUT0
    case BP_REG_LOADTLUT1: // TX_LOADTLUT1
        {
            u32 cnt = ((g_bp_regs.mem[0x65] >> 10) & 0x3ff) << 5;
	        u32 mem_addr = (g_bp_regs.mem[0x64] & 0x1fffff) << 5;
	        u32 tlut_addr = (g_bp_regs.mem[0x65] & 0x3ff) << 5;

	        memcpy(&gp::tmem[tlut_addr & TMEM_MASK], &Mem_RAM[mem_addr & RAM_MASK], cnt);
            LOG_DEBUG(TGP, "BP-> TX_LOADTLUTx");
            break;
        }
        break;



	case 0xe0: // TEV_REGISTERL_0
	case 0xe2: // TEV_REGISTERL_1
	case 0xe4: // TEV_REGISTERL_2
	case 0xe6: // TEV_REGISTERL_3
	case 0xe1: // TEV_REGISTERH_0
	case 0xe3: // TEV_REGISTERH_1
	case 0xe5: // TEV_REGISTERH_2
	case 0xe7: // TEV_REGISTERH_3
		UploadColor(addr, data);
		break;
    }
}

void LoadTexture(u8 num) {
    u32 addr;
    // TODO(ShizZy): Make this a real hash without slowing the emu down terribly. Right now, this 
    // is addr ^ first 4 bytes
    u32 hash = (g_bp_regs.tex[0].image_3[0].image_base ^ *(u32*)&Mem_RAM[addr & RAM_MASK]);

    if(!video_core::g_renderer->BindTexture(hash, num)) {

        int set = (num & 4) >> 2;
        int index = num & 7; 

        DecodeTexture(g_bp_regs.tex[set].image_0[0].format, 
            hash, 
            g_bp_regs.tex[set].image_3[index].get_addr(), 
            g_bp_regs.tex[set].image_0[index].get_height(), 
            g_bp_regs.tex[set].image_0[index].get_width());
    }
    video_core::g_renderer->SetTextureParameters(num);
}

/// Initialize BP
void BPInit() {
    memset(&g_bp_regs, 0, sizeof(g_bp_regs));
}

} // namespace