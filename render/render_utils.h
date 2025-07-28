#pragma once

namespace Render
{
    enum JobType
    {
        Graphics,
        Compute,
        Copy,
    };

	enum class PassType
	{
		Compute,
		DrawMeshes,
		DrawAnimatedMesh,
		FullTriangle,
		Copy,
		SwapChain,
		DepthPrepass,
	};

}   // Render