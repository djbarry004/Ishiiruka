// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "D3DBase.h"
#include "GfxState.h"

namespace DX11
{

namespace D3D
{

StateManager* stateman;

StateManager::StateManager() : cur_blendstate(NULL), cur_depthstate(NULL), cur_raststate(NULL) {}

void StateManager::PushBlendState(const ID3D11BlendState* state) { blendstates.push(state); }
void StateManager::PushDepthState(const ID3D11DepthStencilState* state) { depthstates.push(state); }
void StateManager::PushRasterizerState(const ID3D11RasterizerState* state) { raststates.push(state); }
void StateManager::PopBlendState() { blendstates.pop(); }
void StateManager::PopDepthState() { depthstates.pop(); }
void StateManager::PopRasterizerState() { raststates.pop(); }

void StateManager::Apply()
{
	if (!blendstates.empty())
	{
		if (cur_blendstate != blendstates.top())
		{
			cur_blendstate = (ID3D11BlendState*)blendstates.top();
			D3D::context->OMSetBlendState(cur_blendstate, NULL, 0xFFFFFFFF);
		}
	}
	else ERROR_LOG(VIDEO, "Tried to apply without blend state!");

	if (!depthstates.empty())
	{
		if (cur_depthstate != depthstates.top())
		{
			cur_depthstate = (ID3D11DepthStencilState*)depthstates.top();
			D3D::context->OMSetDepthStencilState(cur_depthstate, 0);
		}
	}
	else ERROR_LOG(VIDEO, "Tried to apply without depth state!");

	if (!raststates.empty())
	{
		if (cur_raststate != raststates.top())
		{
			cur_raststate = (ID3D11RasterizerState*)raststates.top();
			D3D::context->RSSetState(cur_raststate);
		}
	}
	else ERROR_LOG(VIDEO, "Tried to apply without rasterizer state!");
}

}  // namespace

}  // namespace DX11