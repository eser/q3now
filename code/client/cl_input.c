/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"
#include "wired/ui/cl_wired_ui.h"
#include "../qcommon/wired/net/wn_public.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_client, "client" );

extern int cl_sent; /* net-stats packet counter — defined in cl_net_stats.c */

static unsigned frame_msec;
static int old_com_frameTime;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as argv(1) so it can be matched up with the release.

argv(2) will be set to the time the event happened, which allows exact
control even at low framerates when the down and up events may both get queued
at the same time.

===============================================================================
*/

typedef struct {
	int			down[2];		// key nums holding it down
	unsigned	downtime;		// msec timestamp
	unsigned	msec;			// msec down this frame if both a down and up happened
	qboolean	active;			// current state
	qboolean	wasPressed;		// set when down, not cleared when up
} kbutton_t;

static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
static kbutton_t in_strafe, in_speed;
static kbutton_t in_up, in_down;
static kbutton_t in_buttons[16];

static cvar_t *cl_nodelta;

static cvar_t *cl_showSend;

static cvar_t *cl_sensitivity;
static cvar_t *cl_mouseAccel;
static cvar_t *cl_mouseAccelOffset;
static cvar_t *cl_mouseAccelStyle;
static cvar_t *cl_showMouseRate;

static cvar_t *cl_run;
static cvar_t *cl_freelook;

static cvar_t *cl_yawspeed;
static cvar_t *cl_pitchspeed;
static cvar_t *cl_anglespeedkey;

static cvar_t *cl_maxpackets;
static cvar_t *cl_packetdup;

static cvar_t *m_pitch;
static cvar_t *m_yaw;
static cvar_t *m_forward;
static cvar_t *m_side;
static cvar_t *m_filter;

static qboolean in_mlooking;

static void IN_CenterView( void ) {
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.snap.ps.delta_angles[PITCH]);
}

static void IN_MLookDown( void ) {
	in_mlooking = qtrue;
}


static void IN_MLookUp( void ) {
	in_mlooking = qfalse;
	if ( !cl_freelook->integer ) {
		IN_CenterView ();
	}
}


static void IN_KeyDown( kbutton_t *b ) {
	const char *c = Cmd_Argv(1);
	int k;
	if ( c[0] ) {
		k = atoi(c);
	} else {
		k = -1;		// typed manually at the console for continuous down
	}

	if ( k == b->down[0] || k == b->down[1] ) {
		return;		// repeating key
	}

	if ( !b->down[0] ) {
		b->down[0] = k;
	} else if ( !b->down[1] ) {
		b->down[1] = k;
	} else {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Three keys down for a button!\n");
		return;
	}

	if ( b->active ) {
		return;		// still down
	}

	// save timestamp for partial frame summing
	c = Cmd_Argv(2);
	b->downtime = atoi(c);

	b->active = qtrue;
	b->wasPressed = qtrue;
}


static void IN_KeyUp( kbutton_t *b ) {
	const char *c = Cmd_Argv(1);
	if ( c[0] ) {
		int k = atoi(c);
		if ( b->down[0] == k ) {
			b->down[0] = 0;
		} else if ( b->down[1] == k ) {
			b->down[1] = 0;
		} else {
			return;		// key up without corresponding down (menu pass through)
		}
	} else {
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->active = qfalse;
		return;
	}
	if ( b->down[0] || b->down[1] ) {
		return;		// some other key is still holding it down
	}

	b->active = qfalse;

	// save timestamp for partial frame summing
	c = Cmd_Argv(2);
	unsigned uptime = atoi(c);
	if ( uptime ) {
		b->msec += uptime - b->downtime;
	} else {
		b->msec += frame_msec / 2;
	}

	b->active = qfalse;
}


/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
static float CL_KeyState( kbutton_t *key ) {
	int			msec = key->msec;
	key->msec = 0;

	if ( key->active ) {
		// still down
		if ( !key->downtime ) {
			msec = com_frameTime;
		} else {
			msec += com_frameTime - key->downtime;
		}
		key->downtime = com_frameTime;
	}

#if 0
	if (msec) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%i ", msec);
	}
#endif

	float val = (float)msec / frame_msec;
	if ( val < 0 ) {
		val = 0;
	}
	if ( val > 1 ) {
		val = 1;
	}

	return val;
}


static void IN_UpDown(void) {IN_KeyDown(&in_up);}
static void IN_UpUp(void) {IN_KeyUp(&in_up);}
static void IN_DownDown(void) {IN_KeyDown(&in_down);}
static void IN_DownUp(void) {IN_KeyUp(&in_down);}
static void IN_LeftDown(void) {IN_KeyDown(&in_left);}
static void IN_LeftUp(void) {IN_KeyUp(&in_left);}
static void IN_RightDown(void) {IN_KeyDown(&in_right);}
static void IN_RightUp(void) {IN_KeyUp(&in_right);}
static void IN_ForwardDown(void) {IN_KeyDown(&in_forward);}
static void IN_ForwardUp(void) {IN_KeyUp(&in_forward);}
static void IN_BackDown(void) {IN_KeyDown(&in_back);}
static void IN_BackUp(void) {IN_KeyUp(&in_back);}
static void IN_LookupDown(void) {IN_KeyDown(&in_lookup);}
static void IN_LookupUp(void) {IN_KeyUp(&in_lookup);}
static void IN_LookdownDown(void) {IN_KeyDown(&in_lookdown);}
static void IN_LookdownUp(void) {IN_KeyUp(&in_lookdown);}
static void IN_MoveleftDown(void) {IN_KeyDown(&in_moveleft);}
static void IN_MoveleftUp(void) {IN_KeyUp(&in_moveleft);}
static void IN_MoverightDown(void) {IN_KeyDown(&in_moveright);}
static void IN_MoverightUp(void) {IN_KeyUp(&in_moveright);}

static void IN_SpeedDown(void) {IN_KeyDown(&in_speed);}
static void IN_SpeedUp(void) {IN_KeyUp(&in_speed);}
static void IN_StrafeDown(void) {IN_KeyDown(&in_strafe);}
static void IN_StrafeUp(void) {IN_KeyUp(&in_strafe);}

static void IN_Button0Down(void) {IN_KeyDown(&in_buttons[0]);}
static void IN_Button0Up(void) {IN_KeyUp(&in_buttons[0]);}
static void IN_Button1Down(void) {IN_KeyDown(&in_buttons[1]);}
static void IN_Button1Up(void) {IN_KeyUp(&in_buttons[1]);}
static void IN_Button2Down(void) {IN_KeyDown(&in_buttons[2]);}
static void IN_Button2Up(void) {IN_KeyUp(&in_buttons[2]);}
static void IN_Button3Down(void) {IN_KeyDown(&in_buttons[3]);}
static void IN_Button3Up(void) {IN_KeyUp(&in_buttons[3]);}
static void IN_Button4Down(void) {IN_KeyDown(&in_buttons[4]);}
static void IN_Button4Up(void) {IN_KeyUp(&in_buttons[4]);}
static void IN_Button5Down(void) {IN_KeyDown(&in_buttons[5]);}
static void IN_Button5Up(void) {IN_KeyUp(&in_buttons[5]);}
static void IN_Button6Down(void) {IN_KeyDown(&in_buttons[6]);}
static void IN_Button6Up(void) {IN_KeyUp(&in_buttons[6]);}
static void IN_Button7Down(void) {IN_KeyDown(&in_buttons[7]);}
static void IN_Button7Up(void) {IN_KeyUp(&in_buttons[7]);}
static void IN_Button8Down(void) {IN_KeyDown(&in_buttons[8]);}
static void IN_Button8Up(void) {IN_KeyUp(&in_buttons[8]);}
static void IN_Button9Down(void) {IN_KeyDown(&in_buttons[9]);}
static void IN_Button9Up(void) {IN_KeyUp(&in_buttons[9]);}
static void IN_Button10Down(void) {IN_KeyDown(&in_buttons[10]);}
static void IN_Button10Up(void) {IN_KeyUp(&in_buttons[10]);}
static void IN_Button11Down(void) {IN_KeyDown(&in_buttons[11]);}
static void IN_Button11Up(void) {IN_KeyUp(&in_buttons[11]);}
static void IN_Button12Down(void) {IN_KeyDown(&in_buttons[12]);}
static void IN_Button12Up(void) {IN_KeyUp(&in_buttons[12]);}
static void IN_Button13Down(void) {IN_KeyDown(&in_buttons[13]);}
static void IN_Button13Up(void) {IN_KeyUp(&in_buttons[13]);}
static void IN_Button14Down(void) {IN_KeyDown(&in_buttons[14]);}
static void IN_Button14Up(void) {IN_KeyUp(&in_buttons[14]);}
static void IN_Button15Down(void) {IN_KeyDown(&in_buttons[15]);}
static void IN_Button15Up(void) {IN_KeyUp(&in_buttons[15]);}


//==========================================================================


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
static void CL_AdjustAngles( void ) {
	float	speed;

	if ( in_speed.active ) {
		speed = 0.001 * cls.frametime * cl_anglespeedkey->value;
	} else {
		speed = 0.001 * cls.frametime;
	}

	if ( !in_strafe.active ) {
		cl.viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState (&in_right);
		cl.viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState (&in_left);
	}

	cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * CL_KeyState (&in_lookup);
	cl.viewangles[PITCH] += speed*cl_pitchspeed->value * CL_KeyState (&in_lookdown);
}


/*
================
CL_KeyMove

Sets the usercmd_t based on key states
================
*/
static void CL_KeyMove( usercmd_t *cmd ) {
	int		movespeed;

	//
	// adjust for speed key / running
	// the walking flag is to keep animations consistent
	// even during acceleration and deceleration
	//
	if ( in_speed.active ^ cl_run->integer ) {
		movespeed = 127;
		cmd->buttons &= ~BUTTON_WALKING;
	} else {
		cmd->buttons |= BUTTON_WALKING;
		movespeed = 64;
	}

	int forward = 0;
	int side    = 0;
	int up      = 0;
	if ( in_strafe.active ) {
		side += movespeed * CL_KeyState (&in_right);
		side -= movespeed * CL_KeyState (&in_left);
	}

	side += movespeed * CL_KeyState (&in_moveright);
	side -= movespeed * CL_KeyState (&in_moveleft);


	up += movespeed * CL_KeyState (&in_up);
	up -= movespeed * CL_KeyState (&in_down);

	forward += movespeed * CL_KeyState (&in_forward);
	forward -= movespeed * CL_KeyState (&in_back);

	cmd->forwardmove = ClampCharMove( forward );
	cmd->rightmove = ClampCharMove( side );
	cmd->upmove = ClampCharMove( up );
}


/*
=================
CL_MouseEvent
=================
*/
void CL_MouseEvent( int dx, int dy /*, int time*/ ) {
	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		UI_CALL_MOUSE_EVENT( dx, dy );
	} else if ( Key_GetCatcher() & KEYCATCH_CGAME ) {
		VM_Call( cgvm, 2, CG_MOUSE_EVENT, dx, dy );
	} else {
		cl.mouseDx[cl.mouseIndex] += dx;
		cl.mouseDy[cl.mouseIndex] += dy;
	}
}


/*
=================
CL_JoystickEvent

Joystick values stay set until changed
=================
*/
void CL_JoystickEvent( int axis, int value, int time ) {
	if ( axis < 0 || axis >= MAX_JOYSTICK_AXIS ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_JoystickEvent: bad axis %i", axis );
	} else {
		cl.joystickAxis[axis] = value;
	}
}


/*
=================
CL_JoystickMove
=================
*/
static void CL_JoystickMove( usercmd_t *cmd ) {
	//int		movespeed;
	float	anglespeed;

	if ( in_speed.active ^ cl_run->integer ) {
		//movespeed = 2;
	} else {
		//movespeed = 1;
		cmd->buttons |= BUTTON_WALKING;
	}

	if ( in_speed.active ) {
		anglespeed = 0.001 * cls.frametime * cl_anglespeedkey->value;
	} else {
		anglespeed = 0.001 * cls.frametime;
	}

	if ( !in_strafe.active ) {
		cl.viewangles[YAW] += anglespeed * cl_yawspeed->value * cl.joystickAxis[AXIS_SIDE];
	} else {
		cmd->rightmove = ClampCharMove( cmd->rightmove + cl.joystickAxis[AXIS_SIDE] );
	}

	if ( in_mlooking ) {
		cl.viewangles[PITCH] += anglespeed * cl_pitchspeed->value * cl.joystickAxis[AXIS_FORWARD];
	} else {
		cmd->forwardmove = ClampCharMove( cmd->forwardmove + cl.joystickAxis[AXIS_FORWARD] );
	}

	cmd->upmove = ClampCharMove( cmd->upmove + cl.joystickAxis[AXIS_UP] );
}


/*
=================
CL_MouseMove
=================
*/
static void CL_MouseMove( usercmd_t *cmd )
{
	float mx, my;

	// allow mouse smoothing
	if (m_filter->integer)
	{
		mx = (cl.mouseDx[0] + cl.mouseDx[1]) * 0.5f;
		my = (cl.mouseDy[0] + cl.mouseDy[1]) * 0.5f;
	}
	else
	{
		mx = cl.mouseDx[cl.mouseIndex];
		my = cl.mouseDy[cl.mouseIndex];
	}

	cl.mouseIndex ^= 1;
	cl.mouseDx[cl.mouseIndex] = 0;
	cl.mouseDy[cl.mouseIndex] = 0;

	if (mx == 0.0f && my == 0.0f)
		return;

	if ( cl_mouseAccel->value != 0.0f )
	{
		if ( cl_mouseAccelStyle->integer == 0 )
		{
			float accelSensitivity;
			float rate;

			rate = sqrt(mx * mx + my * my) / (float) frame_msec;

			accelSensitivity = cl_sensitivity->value + rate * cl_mouseAccel->value;
			mx *= accelSensitivity;
			my *= accelSensitivity;

			if ( cl_showMouseRate->integer )
				Com_Log( SEV_INFO, LOG_CH(ch_client), "rate: %f, accelSensitivity: %f\n", rate, accelSensitivity );
		}
		else
		{
			float rate[2];
			float power[2];
			float offset = cl_mouseAccelOffset->value;

			// clip at a small positive number to avoid division
			// by zero (or indeed going backwards!)
			if ( offset < 0.001f ) {
				offset = 0.001f;
			}

			// sensitivity remains pretty much unchanged at low speeds
			// cl_mouseAccel is a power value to how the acceleration is shaped
			// cl_mouseAccelOffset is the rate for which the acceleration will have doubled the non accelerated amplification
			// NOTE: decouple the config cvars for independent acceleration setup along X and Y?

			rate[0] = fabsf( mx ) / (float) frame_msec;
			rate[1] = fabsf( my ) / (float) frame_msec;
			power[0] = powf( rate[0] / offset, cl_mouseAccel->value );
			power[1] = powf( rate[1] / offset, cl_mouseAccel->value );

			mx = cl_sensitivity->value * (mx + ((mx < 0) ? -power[0] : power[0]) * offset);
			my = cl_sensitivity->value * (my + ((my < 0) ? -power[1] : power[1]) * offset);

			if(cl_showMouseRate->integer)
				Com_Log( SEV_INFO, LOG_CH(ch_client), "ratex: %f, ratey: %f, powx: %f, powy: %f\n", rate[0], rate[1], power[0], power[1]);
		}
	}
	else
	{
		mx *= cl_sensitivity->value;
		my *= cl_sensitivity->value;
	}

	// ingame FOV
	mx *= cl.cgameSensitivity;
	my *= cl.cgameSensitivity;

	// add mouse X/Y movement to cmd
	if ( in_strafe.active )
		cmd->rightmove = ClampCharMove( cmd->rightmove + m_side->value * mx );
	else
		cl.viewangles[YAW] -= m_yaw->value * mx;

	if ( (in_mlooking || cl_freelook->integer) && !in_strafe.active )
		cl.viewangles[PITCH] += m_pitch->value * my;
	else
		cmd->forwardmove = ClampCharMove( cmd->forwardmove - m_forward->value * my );
}


/*
==============
CL_CmdButtons
==============
*/
static void CL_CmdButtons( usercmd_t *cmd ) {
	//
	// figure button bits
	// send a button bit even if the key was pressed and released in
	// less than a frame
	//
	for ( int i = 0 ; i < ARRAY_LEN( in_buttons ); i++ ) {
		if ( in_buttons[i].active || in_buttons[i].wasPressed ) {
			cmd->buttons |= 1 << i;
		}
		in_buttons[i].wasPressed = qfalse;
	}

	if ( Key_GetCatcher() ) {
		cmd->buttons |= BUTTON_TALK;
	}

	// allow the game to know if any key at all is
	// currently pressed, even if it isn't bound to anything
	if ( anykeydown && Key_GetCatcher() == 0 ) {
		cmd->buttons |= BUTTON_ANY;
	}
}


/*
==============
CL_FinishMove
==============
*/
static void CL_FinishMove( usercmd_t *cmd ) {
	// copy the state that the cgame is currently sending
	cmd->weapon = cl.cgameUserCmdValue;

	// send the current server time so the amount of movement
	// can be determined without allowing cheating
	cmd->serverTime = cl.serverTime;

	for (int i=0 ; i<3 ; i++) {
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
	}
}


/*
=================
CL_CreateCmd
=================
*/
static usercmd_t CL_CreateCmd( void ) {
	usercmd_t	cmd;
	vec3_t		oldAngles;

	VectorCopy( cl.viewangles, oldAngles );

	// keyboard angle adjustment
	CL_AdjustAngles ();

	memset( &cmd, 0, sizeof( cmd ) );

	CL_CmdButtons( &cmd );

	// get basic movement from keyboard
	CL_KeyMove( &cmd );

	// get basic movement from mouse
	CL_MouseMove( &cmd );

	// get basic movement from joystick
	CL_JoystickMove( &cmd );

	// check to make sure the angles haven't wrapped
	if ( cl.viewangles[PITCH] - oldAngles[PITCH] > 90 ) {
		cl.viewangles[PITCH] = oldAngles[PITCH] + 90;
	} else if ( oldAngles[PITCH] - cl.viewangles[PITCH] > 90 ) {
		cl.viewangles[PITCH] = oldAngles[PITCH] - 90;
	}

	// store out the final values
	CL_FinishMove( &cmd );

	// draw debug graphs of turning for mouse testing
	if ( cl_debugMove->integer ) {
		if ( cl_debugMove->integer == 1 ) {
			SCR_DebugGraph( fabsf( cl.viewangles[YAW] - oldAngles[YAW] ) );
		} else if ( cl_debugMove->integer == 2 ) {
			SCR_DebugGraph( fabsf( cl.viewangles[PITCH] - oldAngles[PITCH] ) );
		}
	}

	return cmd;
}


/*
=================
CL_CreateNewCommands

Create a new usercmd_t structure for this frame
=================
*/
static void CL_CreateNewCommands( void ) {
	// no need to create usercmds until we have a gamestate
	if ( cls.state < CA_PRIMED ) {
		return;
	}

	frame_msec = com_frameTime - old_com_frameTime;

	// if running over 1000fps, act as if each frame is 1ms
	// prevents divisions by zero
	if ( frame_msec < 1 ) {
		frame_msec = 1;
	}

	// if running less than 5fps, truncate the extra time to prevent
	// unexpected moves after a hitch
	if ( frame_msec > 200 ) {
		frame_msec = 200;
	}
	old_com_frameTime = com_frameTime;


	// generate a command for this frame
	cl.cmdNumber++;
	int cmdNum = cl.cmdNumber & CMD_MASK;
	cl.cmds[cmdNum] = CL_CreateCmd();
}


/*
=================
CL_ReadyToSendPacket

Returns qfalse if we are over the maxpackets limit
and should choke back the bandwidth a bit by not sending
a packet this frame.  All the commands will still get
delivered in the next packet, but saving a header and
getting more delta compression will reduce total bandwidth.
=================
*/
static qboolean CL_ReadyToSendPacket( void ) {
	// don't send anything if playing back a demo
	if ( clc.demoplaying || cls.state == CA_CINEMATIC ) {
		return qfalse;
	}

	// If we are downloading, we send no less than 50ms between packets
	if ( *clc.downloadTempName && cls.realtime - clc.lastPacketSentTime < 50 ) {
		return qfalse;
	}

	// if we don't have a valid gamestate yet, only send one packet a second
	if ( cls.state != CA_ACTIVE && cls.state != CA_PRIMED &&
		!*clc.downloadTempName &&
		cls.realtime - clc.lastPacketSentTime < RETRANSMIT_TIMEOUT ) {
		return qfalse;
	}

	// send every frame for loopbacks
	if ( clc.netchan.remoteAddress.type == NA_LOOPBACK ) {
		return qtrue;
	}

	// send every frame for LAN
	if ( cl_lanForcePackets->integer && clc.netchan.isLANAddress ) {
		return qtrue;
	}

	int oldPacketNum = (clc.netchan.outgoingSequence - 1) & PACKET_MASK;
	int delta        = cls.realtime - cl.outPackets[ oldPacketNum ].p_realtime;
	if ( delta < 1000 / cl_maxpackets->integer ) {
		// the accumulated commands will go out in the next packet
		return qfalse;
	}

	return qtrue;
}


/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

4	sequence number
2	qport
4	serverid
4	acknowledged sequence number
4	clc.serverCommandSequence
<optional reliable commands>
1	clc_move or clc_moveNoDelta
1	command count
<count * usercmds>

===================
*/
void CL_WritePacket( int repeat ) {
	msg_t		buf;
	byte		data[ MAX_MSGLEN_BUF ];
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;

	// don't send anything if playing back a demo
	if ( clc.demoplaying || cls.state == CA_CINEMATIC ) {
		return;
	}

	memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;

	MSG_Init( &buf, data, MAX_MSGLEN );

	MSG_Bitstream( &buf );
	// write the current serverId so the server
	// can tell if this is from the current gameState
	MSG_WriteLong( &buf, cl.serverId );

	// write the last message we received, which can
	// be used for delta compression, and is also used
	// to tell if we dropped a gamestate
	MSG_WriteLong( &buf, clc.serverMessageSequence );

	// write the last reliable message we received
	MSG_WriteLong( &buf, clc.serverCommandSequence );

	// write any unacknowledged clientCommands
	int n = clc.reliableSequence - clc.reliableAcknowledge;
	if ( clc.quic_conn != CONN_INVALID && transport ) {
		/* QUIC path: send each unacked command on the reliable game-command channel.
		 * Stream guarantees delivery — acknowledge all immediately so
		 * the netchan loop below sends nothing. */
		for ( int ridx = clc.reliableAcknowledge + 1; ridx <= clc.reliableSequence; ridx++ ) {
			const char *cmd = clc.reliableCommands[ridx & (MAX_RELIABLE_COMMANDS - 1)];
			transport->send_reliable( clc.quic_conn, CHAN_COMMANDS,
				(byte *)cmd, (int)strlen( cmd ) + 1 );
		}
		clc.reliableAcknowledge = clc.reliableSequence;
		n = 0; /* skip netchan embedding */
	}
	for ( int i = 0; i < n; i++ ) {
		const int index = clc.reliableAcknowledge + 1 + i;
		MSG_WriteByte( &buf, clc_clientCommand );
		MSG_WriteLong( &buf, index );
		MSG_WriteString( &buf, clc.reliableCommands[ index & ( MAX_RELIABLE_COMMANDS - 1 ) ] );
	}

	if ( clc.quic_conn != CONN_INVALID && transport ) {
		/* QUIC usercmd datagram format:
		 *   [client_tick:u32]    — cl.cmdNumber (newest cmd in this packet)
		 *   [snapshot_ack:u32]   — clc.serverMessageSequence (last snapshot received)
		 *   [serverCmd_ack:u32]  — clc.serverCommandSequence (last server command processed)
		 *   [cmd_count:u8]       — number of delta-compressed cmds following (1-3)
		 *   [delta-cmds...]      — MSG_WriteDeltaUsercmdKey key=0 from nullcmd
		 * Always packs current tick + 2 redundant previous ticks.
		 * key=0 — TLS provides confidentiality; no netchan XOR needed. */
		byte  udg_data[2048];
		msg_t udg;
		int qcount = 3;
		if ( cl.cmdNumber < 3 )
			qcount = cl.cmdNumber > 0 ? cl.cmdNumber : 1;

		MSG_Init( &udg, udg_data, sizeof(udg_data) );
		MSG_Bitstream( &udg );
		/* client_tick:u32 — client's current command number */
		MSG_WriteLong( &udg, cl.cmdNumber );
		/* snapshot_ack:u32 — last server snapshot received (enables delta compression) */
		MSG_WriteLong( &udg, clc.serverMessageSequence );
		/* serverCmd_ack:u32 — last server command processed by client;
		 * server advances reliableAcknowledge to stop re-embedding processed commands */
		MSG_WriteLong( &udg, clc.serverCommandSequence );
		Com_Log( SEV_TRACE, LOG_CH(ch_client), "[WiredNet] usercmd send: snapshot_ack=%d serverCmd_ack=%d\n",
			clc.serverMessageSequence, clc.serverCommandSequence );
		MSG_WriteByte( &udg, qcount );

		oldcmd = &nullcmd;
		for ( int qi = 0; qi < qcount; qi++ ) {
			int qj = (cl.cmdNumber - qcount + qi + 1) & CMD_MASK;
			cmd = &cl.cmds[qj];
			MSG_WriteDeltaUsercmdKey( &udg, 0, oldcmd, cmd );
			oldcmd = cmd;
		}

		if ( !udg.overflowed ) {
			transport->send_unreliable( clc.quic_conn, udg.data, udg.cursize );
		}

		/* keep outPackets ring valid for snapshot interpolation + next-frame count */
		int packetNum = clc.netchan.outgoingSequence & PACKET_MASK;
		cl.outPackets[packetNum].p_realtime   = cls.realtime;
		cl.outPackets[packetNum].p_serverTime = oldcmd->serverTime;
		cl.outPackets[packetNum].p_cmdNumber  = cl.cmdNumber;
		clc.lastPacketSentTime                = cls.realtime;
		clc.netchan.outgoingSequence++;
		return;
	}

	// we want to send all the usercmds that were generated in the last
	// few packet, so even if a couple packets are dropped in a row,
	// all the cmds will make it to the server

	int oldPacketNum = (clc.netchan.outgoingSequence - 1 - cl_packetdup->integer) & PACKET_MASK;
	int count        = cl.cmdNumber - cl.outPackets[ oldPacketNum ].p_cmdNumber;
	if ( count > MAX_PACKET_USERCMDS ) {
		count = MAX_PACKET_USERCMDS;
		Com_Log( SEV_INFO, LOG_CH(ch_client), "MAX_PACKET_USERCMDS\n");
	}
	if ( count >= 1 ) {
		if ( cl_showSend->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "(%i)", count );
		}

		// begin a client move command
		if ( cl_nodelta->integer || !cl.snap.valid || clc.demowaiting || clc.serverMessageSequence != cl.snap.messageNum ) {
			MSG_WriteByte( &buf, clc_moveNoDelta );
		} else {
			MSG_WriteByte( &buf, clc_move );
		}

		// write the command count
		MSG_WriteByte( &buf, count );

		// use the checksum feed in the key
		int key = clc.checksumFeed;
		// also use the message acknowledge
		key ^= clc.serverMessageSequence;
		// also use the last acknowledged server command in the key
		key ^= MSG_HashKey(clc.serverCommands[ clc.serverCommandSequence & (MAX_RELIABLE_COMMANDS-1) ], 32);

		// write all the commands, including the predicted command
		for ( int i = 0 ; i < count ; i++ ) {
			int j = (cl.cmdNumber - count + i + 1) & CMD_MASK;
			cmd = &cl.cmds[j];
			MSG_WriteDeltaUsercmdKey (&buf, key, oldcmd, cmd);
			oldcmd = cmd;
		}
	}

	//
	// deliver the message
	//
	int packetNum = clc.netchan.outgoingSequence & PACKET_MASK;
	cl.outPackets[ packetNum ].p_realtime = cls.realtime;
	cl.outPackets[ packetNum ].p_serverTime = oldcmd->serverTime;
	cl.outPackets[ packetNum ].p_cmdNumber = cl.cmdNumber;
	clc.lastPacketSentTime = cls.realtime;
	cl_sent++;

	if ( cl_showSend->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%i ", buf.cursize );
	}

	MSG_WriteByte( &buf, clc_EOF );

	if ( buf.overflowed ) {
		if ( cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC ) {
			cls.state = CA_CONNECTING; // to avoid recursive error
		}
		Com_Terminate( TERM_CLIENT_DROP, "%s: message overflowed", __func__ );
	}

	/* Phase D: netchan transmit removed — QUIC datagram path returned early above */
}


/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void ) {
	// don't send any message if not connected
	if ( cls.state < CA_CONNECTED ) {
		return;
	}

	// don't send commands if paused
	if ( com_sv_running->integer && sv_paused->integer && cl_paused->integer ) {
		return;
	}

	// we create commands even if a demo is playing,
	CL_CreateNewCommands();

	// don't send a packet if the last packet was sent too recently
	if ( !CL_ReadyToSendPacket() ) {
		if ( cl_showSend->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), ". " );
		}
		return;
	}

	CL_WritePacket( 0 );
}


static const cvarDesc_t inputDescs[] = {
	/* 0  */ CVAR_BOOL(  "cl_nodelta",          "0",     CVAR_CHEAT,  	  "Flag server to disable delta compression on server snapshots." ),
	/* 1  */ CVAR_INT(   "cl_debugMove",        "0",     0,               "Prints a graph of view angle deltas.\n 0: Disabled\n 1: Yaw\n 2: Pitch", 0, 2 ),
	/* 2  */ CVAR_BOOL(  "cl_showSend",         "0",     CVAR_TEMP,       "Prints client to server packet information." ),
	/* 3  */ CVAR_FLOAT( "cl_yawspeed",         "140",   CVAR_ARCHIVE | CVAR_NODEFAULT, "Side-to-side turning speed using keys (+left and +right).", 0, 0 ),
	/* 4  */ CVAR_FLOAT( "cl_pitchspeed",       "140",   CVAR_ARCHIVE | CVAR_NODEFAULT, "Up and down pitching speed using keys (+lookup and +lookdown).", 0, 0 ),
	/* 5  */ CVAR_FLOAT( "cl_anglespeedkey",    "1.5",   0,               "Set the speed that the direction keys (not mouse) change the view angle.", 0, 0 ),
	/* 6  */ CVAR_INT(   "cl_maxpackets",       "125",   CVAR_ARCHIVE,    "Set how many client packets are sent to the server per second, can't exceed \\com_maxFPS.", 15, 125 ),
	/* 7  */ CVAR_INT(   "cl_packetdup",        "1",     CVAR_ARCHIVE | CVAR_NODEFAULT, "Limits the number of previous client commands added in packet, helps in packet loss mitigation, increases client command packets size a bit.", 0, 5 ),
	/* 8  */ CVAR_BOOL(  "cl_run",              "1",     CVAR_ARCHIVE | CVAR_NODEFAULT, "Persistent player running movement." ),
	/* 9  */ CVAR_FLOAT( "sensitivity",         "5",     CVAR_ARCHIVE,    "Sets base mouse sensitivity (mouse speed).", 0, 0 ),
	/* 10 */ CVAR_FLOAT( "cl_mouseAccel",       "0",     CVAR_ARCHIVE | CVAR_NODEFAULT, "Toggle the use of mouse acceleration the mouse speeds up or becomes more sensitive as it continues in one direction.", 0, 0 ),
	/* 11 */ CVAR_BOOL(  "cl_freelook",         "1",     CVAR_ARCHIVE | CVAR_NODEFAULT, "Allow pitching or up/down look with mouse." ),
	/* 12 */ CVAR_BOOL(  "cl_mouseAccelStyle",  "0",     CVAR_ARCHIVE | CVAR_NODEFAULT, "Choose between two different mouse acceleration styles." ),
	/* 13 */ CVAR_FLOAT( "cl_mouseAccelOffset", "5",     CVAR_ARCHIVE | CVAR_NODEFAULT, "Sets how much base mouse delta will be doubled by acceleration. Requires 'cl_mouseAccelStyle 1'.", 0.001f, 50000 ),
	/* 14 */ CVAR_BOOL(  "cl_showMouseRate",    "0",     0,               "Prints mouse acceleration info when 'cl_mouseAccel' has a value set (rate of mouse samples per frame)." ),
	/* 15 */ CVAR_FLOAT( "m_pitch",             "0.022", CVAR_ARCHIVE | CVAR_NODEFAULT, "Set the up and down movement distance of the player in relation to how much the mouse moves.", 0, 0 ),
	/* 16 */ CVAR_FLOAT( "m_yaw",               "0.022", CVAR_ARCHIVE | CVAR_NODEFAULT, "Set the speed at which the player's screen moves left and right while using the mouse.", 0, 0 ),
	/* 17 */ CVAR_FLOAT( "m_forward",           "0.25",  CVAR_ARCHIVE | CVAR_NODEFAULT, "Set the back and forth movement distance of the player in relation to how much the mouse moves.", 0, 0 ),
	/* 18 */ CVAR_FLOAT( "m_side",              "0.25",  CVAR_ARCHIVE | CVAR_NODEFAULT, "Set the strafe movement distance of the player in relation to how much the mouse moves.", 0, 0 ),
};

enum {
	INP_NODELTA, INP_DEBUGMOVE, INP_SHOWSEND,
	INP_YAWSPEED, INP_PITCHSPEED, INP_ANGLESPEEDKEY,
	INP_MAXPACKETS, INP_PACKETDUP,
	INP_RUN, INP_SENSITIVITY, INP_MOUSEACCEL, INP_FREELOOK,
	INP_MOUSEACCELSTYLE, INP_MOUSEACCELOFFSET, INP_SHOWMOUSERATE,
	INP_MPITCH, INP_MYAW, INP_MFORWARD, INP_MSIDE,
	INP_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( inputDescs ) == INP_CVAR_COUNT, "inputDescs/enum mismatch" );
static cvar_t *inputHandles[INP_CVAR_COUNT];


/*
============
CL_InitInput
============
*/
void CL_InitInput( void ) {
	Cmd_AddCommand ("centerview",IN_CenterView);

	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_Button0Down);
	Cmd_AddCommand ("-attack", IN_Button0Up);
	Cmd_AddCommand ("+attackalt", IN_Button1Down);
	Cmd_AddCommand ("-attackalt", IN_Button1Up);
	Cmd_AddCommand ("+use", IN_Button2Down);
	Cmd_AddCommand ("-use", IN_Button2Up);
	Cmd_AddCommand ("+gesture1", IN_Button3Down);
	Cmd_AddCommand ("-gesture1", IN_Button3Up);
	Cmd_AddCommand ("+button0", IN_Button0Down);
	Cmd_AddCommand ("-button0", IN_Button0Up);
	Cmd_AddCommand ("+button1", IN_Button1Down);
	Cmd_AddCommand ("-button1", IN_Button1Up);
	Cmd_AddCommand ("+button2", IN_Button2Down);
	Cmd_AddCommand ("-button2", IN_Button2Up);
	Cmd_AddCommand ("+button3", IN_Button3Down);
	Cmd_AddCommand ("-button3", IN_Button3Up);
	Cmd_AddCommand ("+button4", IN_Button4Down);
	Cmd_AddCommand ("-button4", IN_Button4Up);
	Cmd_AddCommand ("+button5", IN_Button5Down);
	Cmd_AddCommand ("-button5", IN_Button5Up);
	Cmd_AddCommand ("+button6", IN_Button6Down);
	Cmd_AddCommand ("-button6", IN_Button6Up);
	Cmd_AddCommand ("+button7", IN_Button7Down);
	Cmd_AddCommand ("-button7", IN_Button7Up);
	Cmd_AddCommand ("+button8", IN_Button8Down);
	Cmd_AddCommand ("-button8", IN_Button8Up);
	Cmd_AddCommand ("+button9", IN_Button9Down);
	Cmd_AddCommand ("-button9", IN_Button9Up);
	Cmd_AddCommand ("+button10", IN_Button10Down);
	Cmd_AddCommand ("-button10", IN_Button10Up);
	Cmd_AddCommand ("+button11", IN_Button11Down);
	Cmd_AddCommand ("-button11", IN_Button11Up);
	Cmd_AddCommand ("+button12", IN_Button12Down);
	Cmd_AddCommand ("-button12", IN_Button12Up);
	Cmd_AddCommand ("+button13", IN_Button13Down);
	Cmd_AddCommand ("-button13", IN_Button13Up);
	Cmd_AddCommand ("+button14", IN_Button14Down);
	Cmd_AddCommand ("-button14", IN_Button14Up);
	Cmd_AddCommand ("+button15", IN_Button15Down);
	Cmd_AddCommand ("-button15", IN_Button15Up);

	Cvar_RegisterTable( inputDescs, ARRAY_LEN( inputDescs ), inputHandles );
	cl_nodelta         = inputHandles[INP_NODELTA];
	cl_debugMove       = inputHandles[INP_DEBUGMOVE];
	cl_showSend        = inputHandles[INP_SHOWSEND];
	cl_yawspeed        = inputHandles[INP_YAWSPEED];
	cl_pitchspeed      = inputHandles[INP_PITCHSPEED];
	cl_anglespeedkey   = inputHandles[INP_ANGLESPEEDKEY];
	cl_maxpackets      = inputHandles[INP_MAXPACKETS];
	cl_packetdup       = inputHandles[INP_PACKETDUP];
	cl_run             = inputHandles[INP_RUN];
	cl_sensitivity     = inputHandles[INP_SENSITIVITY];
	cl_mouseAccel      = inputHandles[INP_MOUSEACCEL];
	cl_freelook        = inputHandles[INP_FREELOOK];
	cl_mouseAccelStyle  = inputHandles[INP_MOUSEACCELSTYLE];
	cl_mouseAccelOffset = inputHandles[INP_MOUSEACCELOFFSET];
	cl_showMouseRate   = inputHandles[INP_SHOWMOUSERATE];
	m_pitch            = inputHandles[INP_MPITCH];
	m_yaw              = inputHandles[INP_MYAW];
	m_forward          = inputHandles[INP_MFORWARD];
	m_side             = inputHandles[INP_MSIDE];

	{
#ifdef MACOS_X
		// Input is jittery on OS X w/o this
		static const cvarDesc_t d = CVAR_BOOL( "m_filter", "1", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Toggle use of mouse 'smoothing'." );
#else
		static const cvarDesc_t d = CVAR_BOOL( "m_filter", "0", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Toggle use of mouse 'smoothing'." );
#endif
		m_filter = Cvar_Register( &d );
	}
}


/*
============
CL_ClearInput
============
*/
void CL_ClearInput( void ) {
	Cmd_RemoveCommand ("centerview");

	Cmd_RemoveCommand ("+moveup");
	Cmd_RemoveCommand ("-moveup");
	Cmd_RemoveCommand ("+movedown");
	Cmd_RemoveCommand ("-movedown");
	Cmd_RemoveCommand ("+left");
	Cmd_RemoveCommand ("-left");
	Cmd_RemoveCommand ("+right");
	Cmd_RemoveCommand ("-right");
	Cmd_RemoveCommand ("+forward");
	Cmd_RemoveCommand ("-forward");
	Cmd_RemoveCommand ("+back");
	Cmd_RemoveCommand ("-back");
	Cmd_RemoveCommand ("+lookup");
	Cmd_RemoveCommand ("-lookup");
	Cmd_RemoveCommand ("+lookdown");
	Cmd_RemoveCommand ("-lookdown");
	Cmd_RemoveCommand ("+moveleft");
	Cmd_RemoveCommand ("-moveleft");
	Cmd_RemoveCommand ("+moveright");
	Cmd_RemoveCommand ("-moveright");
	Cmd_RemoveCommand ("+speed");
	Cmd_RemoveCommand ("-speed");
	Cmd_RemoveCommand ("+attack");
	Cmd_RemoveCommand ("-attack");
	Cmd_RemoveCommand ("+button0");
	Cmd_RemoveCommand ("-button0");
	Cmd_RemoveCommand ("+button1");
	Cmd_RemoveCommand ("-button1");
	Cmd_RemoveCommand ("+button2");
	Cmd_RemoveCommand ("-button2");
	Cmd_RemoveCommand ("+button3");
	Cmd_RemoveCommand ("-button3");
	Cmd_RemoveCommand ("+button4");
	Cmd_RemoveCommand ("-button4");
	Cmd_RemoveCommand ("+button5");
	Cmd_RemoveCommand ("-button5");
	Cmd_RemoveCommand ("+button6");
	Cmd_RemoveCommand ("-button6");
	Cmd_RemoveCommand ("+button7");
	Cmd_RemoveCommand ("-button7");
	Cmd_RemoveCommand ("+button8");
	Cmd_RemoveCommand ("-button8");
	Cmd_RemoveCommand ("+button9");
	Cmd_RemoveCommand ("-button9");
	Cmd_RemoveCommand ("+button10");
	Cmd_RemoveCommand ("-button10");
	Cmd_RemoveCommand ("+button11");
	Cmd_RemoveCommand ("-button11");
	Cmd_RemoveCommand ("+button12");
	Cmd_RemoveCommand ("-button12");
	Cmd_RemoveCommand ("+button13");
	Cmd_RemoveCommand ("-button13");
	Cmd_RemoveCommand ("+button14");
	Cmd_RemoveCommand ("-button14");
	Cmd_RemoveCommand ("+button15");
	Cmd_RemoveCommand ("-button15");
}
