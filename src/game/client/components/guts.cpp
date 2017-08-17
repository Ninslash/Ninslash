#include <base/math.h>
#include <engine/graphics.h>
#include <engine/demo.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <game/client/customstuff.h>
#include <game/gamecore.h>
#include "guts.h"


inline vec2 RandomDir() { return normalize(vec2(frandom()-0.5f, frandom()-0.5f)); }


CGuts::CGuts()
{
	OnReset();
	m_RenderGuts.m_pParts = this;
}


void CGuts::OnReset()
{
	// reset blood
	for(int i = 0; i < MAX_GUTS; i++)
	{
		m_aGuts[i].m_PrevPart = i-1;
		m_aGuts[i].m_NextPart = i+1;
	}

	m_aGuts[0].m_PrevPart = 0;
	m_aGuts[MAX_GUTS-1].m_NextPart = -1;
	m_FirstFree = 0;

	for(int i = 0; i < NUM_GROUPS; i++)
		m_aFirstPart[i] = -1;
}

void CGuts::Add(int Group, CGutSpill *pPart)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		if(pInfo->m_Paused)
			return;
	}
	else
	{
		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
			return;
	}

	if (m_FirstFree == -1)
		return;

	// remove from the free list
	int Id = m_FirstFree;
	m_FirstFree = m_aGuts[Id].m_NextPart;
	if(m_FirstFree != -1)
		m_aGuts[m_FirstFree].m_PrevPart = -1;

	// copy data
	m_aGuts[Id] = *pPart;

	// insert to the group list
	m_aGuts[Id].m_PrevPart = -1;
	m_aGuts[Id].m_NextPart = m_aFirstPart[Group];
	if(m_aFirstPart[Group] != -1)
		m_aGuts[m_aFirstPart[Group]].m_PrevPart = Id;
	m_aFirstPart[Group] = Id;

	// set some parameters
	m_aGuts[Id].m_Life = 0;
}


void CGuts::Bounce(vec2 Pos, vec2 Dir, int Group, vec4 Color)
{
	/*
	CGutSpill b;
	b.SetDefault();
	b.m_Pos = Pos;
	
	b.m_Spr = SPRITE_BLOOD01;
	b.m_LifeSpan = 2.0f + frandom()*2.0f;
	b.m_Rotspeed = 0.0f;
	b.m_StartSize = (30.0f + frandom()*24) / 1.75f;
	b.m_EndSize = 16.0f / 1.75f;
	b.m_Gravity = 1500.0f + frandom()*400;
	b.m_Friction = 0.7f+frandom()*0.075f;

	if (g_Config.m_GfxMultiBuffering)
	{
		b.m_Rotspeed = 0.0f;
		//b.m_StartSize *= 1.5f;
		b.m_StartSize = 42.0f + frandom()*16;
		b.m_EndSize = 4.0f;
		b.m_LifeSpan = 4.0f;
		b.m_Friction = 0.85f+frandom()*0.075f;
	}
	
	b.m_Vel = Dir * ((frandom()+0.15f)*700.0f);
	b.m_Rot = GetAngle(b.m_Vel);
	
	//float c = frandom()*0.3f + 0.7f;
	//b.m_Color = vec4(c, c, c, 1.0f);
	b.m_Color = Color;
	m_pClient->m_pGuts->Add(Group, &b);
	
	if (g_Config.m_GfxMultiBuffering && frandom()*10 < 3 && Group == GROUP_GUTS)
		m_pClient->m_pEffects->Splatter(b.m_Pos + Dir*frandom()*48.0f - Dir*frandom()*16.0f, b.m_Rot, b.m_StartSize * 2, Color);
	*/
}


void CGuts::Update(float TimePassed)
{
	static float FrictionFraction = 0;
	FrictionFraction += TimePassed;

	if(FrictionFraction > 2.0f) // safty messure
		FrictionFraction = 0;

	int FrictionCount = 0;
	while(FrictionFraction > 0.05f)
	{
		FrictionCount++;
		//FrictionFraction -= 0.05f;
		FrictionFraction -= 0.075f;
	}

	for(int g = 0; g < NUM_GROUPS; g++)
	{
		int i = m_aFirstPart[g];
		while(i != -1)
		{
			int Next = m_aGuts[i].m_NextPart;
			//m_aGuts[i].vel += flow_get(m_aGuts[i].pos)*time_passed * m_aGuts[i].flow_affected;
			
			for (int p = 0; p < 5; p++)
			{
				m_aGuts[i].m_aVel[p].y += m_aGuts[i].m_Gravity*TimePassed;

				// ugly way to force tiles to blood
				int OnForceTile = Collision()->IsForceTile(m_aGuts[i].m_aPos[p].x, m_aGuts[i].m_aPos[p].y+4);
				
				for(int f = 0; f < FrictionCount; f++) // apply friction
					m_aGuts[i].m_aVel[p] *= m_aGuts[i].m_Friction;
				
				if (OnForceTile != 0)
					m_aGuts[i].m_aVel[p].x = OnForceTile*250;
					
				vec2 Force = m_aGuts[i].m_aVel[p];
					
				if (CustomStuff()->Impact(m_aGuts[i].m_aPos[p], &Force))
				{
					m_aGuts[i].m_aPos[p] += Force*10.0f;
					m_aGuts[i].m_aVel[p] += Force*(700.0f+frandom()*700);
					
					if (frandom()*20 < 2)
						m_pClient->AddPlayerSplatter(m_aGuts[i].m_aPos[p], m_aGuts[i].m_Color);
				}
				
				
				if (p == 0)
				{
					vec2 n = m_aGuts[i].m_aPos[p] - m_aGuts[i].m_aPos[p+1];
					float d = abs(length(n));
					
					if (d > 20)
						m_aGuts[i].m_aVel[p] -= n*0.5f;
					
					if (frandom() < TimePassed*10 && m_aGuts[i].m_Life < 1.0f)
						m_pClient->m_pEffects->Blood(m_aGuts[i].m_aPos[p], normalize(n)*4.0f + m_aGuts[i].m_aVel[p]/2, m_aGuts[i].m_Color);
				}
				else if (p == 4)
				{
					vec2 n = m_aGuts[i].m_aPos[p-1] - m_aGuts[i].m_aPos[p];
					float d = abs(length(n));
					
					if (d > 20)
						m_aGuts[i].m_aVel[p] += n*0.5f;
				}
				else
				{
					vec2 n = m_aGuts[i].m_aPos[p] - m_aGuts[i].m_aPos[p-1];
					float d = abs(length(n));
					
					if (d > 20)
						m_aGuts[i].m_aVel[p] -= n*0.5f;
					
					if (d < 10)
						m_aGuts[i].m_aVel[p] += n*0.3f;
					
					n = m_aGuts[i].m_aPos[p+1] - m_aGuts[i].m_aPos[p];
					d = abs(length(n));
					
					if (d > 20)
						m_aGuts[i].m_aVel[p] += n*0.5f;
					
					if (d < 10)
						m_aGuts[i].m_aVel[p] -= n*0.3f;
				}

				
				// move the point
				vec2 Vel = m_aGuts[i].m_aVel[p]*TimePassed;
				
				vec2 OldVel = Vel;
				//Vel.x += OnForceTile*1;

				if (OnForceTile != 0)
					Collision()->MoveBox(&m_aGuts[i].m_aPos[p], &Vel, vec2(6, 6), 0.99f, false);
				else
					Collision()->MoveBox(&m_aGuts[i].m_aPos[p], &Vel, vec2(6, 6), 0.85f, false);
				
				// stick to walls and ceiling
				vec2 P = m_aGuts[i].m_aPos[p];
				

				m_aGuts[i].m_aVel[p] = Vel* (1.0f/TimePassed);
			}

			m_aGuts[i].m_Life += TimePassed;

				
			// check blood death
			if(m_aGuts[i].m_Life > m_aGuts[i].m_LifeSpan)
			{
				// remove it from the group list
				if(m_aGuts[i].m_PrevPart != -1)
					m_aGuts[m_aGuts[i].m_PrevPart].m_NextPart = m_aGuts[i].m_NextPart;
				else
					m_aFirstPart[g] = m_aGuts[i].m_NextPart;

				if(m_aGuts[i].m_NextPart != -1)
					m_aGuts[m_aGuts[i].m_NextPart].m_PrevPart = m_aGuts[i].m_PrevPart;

				// insert to the free list
				if(m_FirstFree != -1)
					m_aGuts[m_FirstFree].m_PrevPart = i;
				m_aGuts[i].m_PrevPart = -1;
				m_aGuts[i].m_NextPart = m_FirstFree;
				m_FirstFree = i;
			}

			i = Next;
		}
	}
}

void CGuts::OnRender()
{
	if(Client()->State() < IClient::STATE_ONLINE)
		return;

	static int64 LastTime = 0;
	int64 t = time_get();

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		if(!pInfo->m_Paused)
			Update((float)((t-LastTime)/(double)time_freq())*pInfo->m_Speed);
	}
	else
	{
		if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
			Update((float)((t-LastTime)/(double)time_freq()));
	}

	LastTime = t;
}


void CGuts::RenderGroup(int Group)
{
	if (Group == GROUP_GUTS)
	{
		/*
		Graphics()->BlendNormal();
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUTS].m_Id);
		Graphics()->QuadsBegin();
		
		int i = m_aFirstPart[Group];
		while(i != -1)
		{
			float a = m_aGuts[i].m_Life / m_aGuts[i].m_LifeSpan;
			vec2 p = m_aGuts[i].m_Pos;

			float Size = mix(m_aGuts[i].m_StartSize, m_aGuts[i].m_EndSize*1.0f, a);
			//RenderTools()->SelectSprite(m_aGuts[i].m_Spr + a*m_aGuts[i].m_Frames);
			Graphics()->QuadsSetRotation(m_aGuts[i].m_Rot);
			//Graphics()->SetColor(m_aGuts[i].m_Color.r, m_aGuts[i].m_Color.g, m_aGuts[i].m_Color.b, 1);
			Graphics()->SetColor(1, 1, 1, 1);
			IGraphics::CQuadItem QuadItem(p.x, p.y, Size, Size);
			Graphics()->QuadsDraw(&QuadItem, 1);

			i = m_aGuts[i].m_NextPart;
		}
		Graphics()->QuadsEnd();
		*/
		
		Graphics()->BlendNormal();
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUTS].m_Id);
		//Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
				
		Graphics()->QuadsSetRotation(0);
		
		int i = m_aFirstPart[Group];
		while(i != -1)
		{
			float a = m_aGuts[i].m_Life / m_aGuts[i].m_LifeSpan;
			//Graphics()->SetColor(1, 1, 1, 1-a);
			Graphics()->SetColor(m_aGuts[i].m_Color.r*0.6f + 0.4f, m_aGuts[i].m_Color.g*0.6f + 0.4f, m_aGuts[i].m_Color.b*0.7f + 0.3f, 1-a);
			
			vec2 aPos[9];
			
			for (int f = 0; f < 5; f++)
				aPos[f*2] = m_aGuts[i].m_aPos[f]+vec2(0, 6);
			
			for (int f = 0; f < 4; f++)
				aPos[1+f*2] = (aPos[f*2] + aPos[2+f*2]) / 2;
			
			for (int f = 1; f < 4; f++)
				aPos[f*2] = (aPos[f*2] + (aPos[f*2-1]+aPos[f*2+1])/2) / 2;
				
			/*
			aPos[1] = (aPos[0] + aPos[2]) / 2;
			aPos[3] = (aPos[2] + aPos[4]) / 2;
			
			aPos[2] = (aPos[2] + (aPos[1]+aPos[3])/2) / 2;
			*/
			
			vec2 p0 = aPos[0]-aPos[1];
			float a0 = atan2(p0.y, p0.x);
				
			float Frames = 8.0f;
				
			for (int f = 0; f < 8; f++)
			{
				//RenderTools()->SelectSprite(SPRITE_FLAME1+f, 0, 0, 0, x1, x2);
			
				p0 = aPos[f]-aPos[f+1];
				
				float a = atan2(p0.y, p0.x);
				
				float a1 = a0-pi/2.0f;
				float a2 = a-pi/2.0f;
				float a3 = a0+pi/2.0f;
				float a4 = a+pi/2.0f;
				
				float s1 = 4.0f;
						
				vec2 p1 = aPos[f]+vec2(cos(a1), sin(a1))*s1;
				vec2 p2 = aPos[f+1]+vec2(cos(a2), sin(a2))*s1;
				vec2 p3 = aPos[f]+vec2(cos(a3), sin(a3))*s1;
				vec2 p4 = aPos[f+1]+vec2(cos(a4), sin(a4))*s1;
			
			/*
				vec2 p1 = aPos[f]+vec2(-4, 4);
				vec2 p2 = aPos[f+1]+vec2(+4, -4);
				vec2 p3 = aPos[f]+vec2(-4, 4);
				vec2 p4 = aPos[f+1]+vec2(+4, 4);
				*/
				
				
				Graphics()->QuadsSetSubsetFree(	f/Frames, 0, 
												(f+1)/Frames, 0, 
												f/Frames, 1, 
												(f+1)/Frames, 1);
			
				IGraphics::CFreeformItem FreeFormItem(
					p1.x, p1.y,
					p2.x, p2.y,
					p3.x, p3.y,
					p4.x, p4.y);
							
				Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
				
				a0 = a;
			}
			
			i = m_aGuts[i].m_NextPart;
		}
		
		Graphics()->QuadsEnd();
	}
}














