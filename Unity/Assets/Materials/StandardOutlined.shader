// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "Standard (Outlined)" {

	Properties
	{
		_Color("Color", Color) = (1,1,1,1)
		_MainTex("Albedo", 2D) = "white" {}

		_Cutoff("Alpha Cutoff", Range(0.0, 1.0)) = 0.5

		_Glossiness("Smoothness", Range(0.0, 1.0)) = 0.5
		[Gamma] _Metallic("Metallic", Range(0.0, 1.0)) = 0.0
		_MetallicGlossMap("Metallic", 2D) = "white" {}

		_BumpScale("Scale", Float) = 1.0
		_BumpMap("Normal Map", 2D) = "bump" {}

		_Parallax("Height Scale", Range(0.005, 0.08)) = 0.02
		_ParallaxMap("Height Map", 2D) = "black" {}

		_OcclusionStrength("Strength", Range(0.0, 1.0)) = 1.0
		_OcclusionMap("Occlusion", 2D) = "white" {}

		_EmissionColor("Color", Color) = (0,0,0)
		_EmissionMap("Emission", 2D) = "white" {}

		_DetailMask("Detail Mask", 2D) = "white" {}

		_DetailAlbedoMap("Detail Albedo x2", 2D) = "grey" {}
		_DetailNormalMapScale("Scale", Float) = 1.0
		_DetailNormalMap("Normal Map", 2D) = "bump" {}

		[Enum(UV0,0,UV1,1)] _UVSec("UV Set for secondary textures", Float) = 0

			// UI-only data
			[HideInInspector] _EmissionScaleUI("Scale", Float) = 0.0
			[HideInInspector] _EmissionColorUI("Color", Color) = (1,1,1)

			// Blending state
			[HideInInspector] _Mode("__mode", Float) = 0.0
			[HideInInspector] _SrcBlend("__src", Float) = 1.0
			[HideInInspector] _DstBlend("__dst", Float) = 0.0
			[HideInInspector] _ZWrite("__zw", Float) = 1.0

			// -------------------------
			// Added Outline properties
			_OutlineColor("Outline Color", Color) = (0,0,0,1)
			_Outline("Outline width", Range(.002, 0.03)) = .005
			// -------------------------
	}

		CGINCLUDE
#define UNITY_SETUP_BRDF_INPUT MetallicSetup
			ENDCG

			/////////////////////////////////////////////////////////////////////////////////////////////

			SubShader
		{
			Tags { "RenderType" = "Opaque" "PerformanceChecks" = "False" }
			LOD 300

			// ----------------------
			// Start of Outline adding

			CGINCLUDE
			#include "UnityCG.cginc"

			struct appdata {
				float4 vertex : POSITION;
				float3 normal : NORMAL;
			};

			struct v2f {
				float4 pos : SV_POSITION;
				UNITY_FOG_COORDS(0)
				fixed4 color : COLOR;
			};

			uniform float _Outline;
			uniform float4 _OutlineColor;

			v2f vert(appdata v) {
				// just make a copy of incoming vertex data but scaled according to normal direction
				v2f o;
				o.pos = UnityObjectToClipPos(v.vertex);

				float3 norm = normalize(mul((float3x3)UNITY_MATRIX_IT_MV, v.normal));
				float2 offset = TransformViewToProjection(norm.xy);

				o.pos.xy += offset * o.pos.z * _Outline;
				o.color = _OutlineColor;
				UNITY_TRANSFER_FOG(o,o.pos);
				return o;
			}
			ENDCG

			Pass {
				Name "OUTLINE"
				Tags { "LightMode" = "Always" }
				Cull Front
				ZWrite On
				ColorMask RGB
				Blend SrcAlpha OneMinusSrcAlpha

				CGPROGRAM
				#pragma vertex vert
				#pragma fragment frag
				#pragma multi_compile_fog
				fixed4 frag(v2f i) : SV_Target
				{
					UNITY_APPLY_FOG(i.fogCoord, i.color);
					return i.color;
				}
				ENDCG
			}

				// End of Outline adding
				// ----------------------


				// ------------------------------------------------------------------
				//  Base forward pass (directional light, emission, lightmaps, ...)
				Pass
				{
					Name "FORWARD"
					Tags { "LightMode" = "ForwardBase" }

					Blend[_SrcBlend][_DstBlend]
					ZWrite[_ZWrite]

					CGPROGRAM
					#pragma target 3.0
					// TEMPORARY: GLES2.0 temporarily disabled to prevent errors spam on devices without textureCubeLodEXT
					#pragma exclude_renderers gles

					// -------------------------------------

					#pragma shader_feature _NORMALMAP
					#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
					#pragma shader_feature _EMISSION
					#pragma shader_feature _METALLICGLOSSMAP
					#pragma shader_feature ___ _DETAIL_MULX2
					#pragma shader_feature _PARALLAXMAP

					#pragma multi_compile_fwdbase
					#pragma multi_compile_fog

					#pragma vertex vertForwardBase
					#pragma fragment fragForwardBase

					#include "UnityStandardCore.cginc"

					ENDCG
				}
					// ------------------------------------------------------------------
					//  Additive forward pass (one light per pass)
					Pass
					{
						Name "FORWARD_DELTA"
						Tags { "LightMode" = "ForwardAdd" }
						Blend[_SrcBlend] One
						Fog { Color(0,0,0,0) } // in additive pass fog should be black
						ZWrite Off
						ZTest LEqual

						CGPROGRAM
						#pragma target 3.0
					// GLES2.0 temporarily disabled to prevent errors spam on devices without textureCubeLodEXT
					#pragma exclude_renderers gles

					// -------------------------------------


					#pragma shader_feature _NORMALMAP
					#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
					#pragma shader_feature _METALLICGLOSSMAP
					#pragma shader_feature ___ _DETAIL_MULX2
					#pragma shader_feature _PARALLAXMAP

					#pragma multi_compile_fwdadd_fullshadows
					#pragma multi_compile_fog

					#pragma vertex vertForwardAdd
					#pragma fragment fragForwardAdd

					#include "UnityStandardCore.cginc"

					ENDCG
				}
					// ------------------------------------------------------------------
					//  Shadow rendering pass
					Pass {
						Name "ShadowCaster"
						Tags { "LightMode" = "ShadowCaster" }

						ZWrite On ZTest LEqual

						CGPROGRAM
						#pragma target 3.0
					// TEMPORARY: GLES2.0 temporarily disabled to prevent errors spam on devices without textureCubeLodEXT
					#pragma exclude_renderers gles

					// -------------------------------------


					#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
					#pragma multi_compile_shadowcaster

					#pragma vertex vertShadowCaster
					#pragma fragment fragShadowCaster

					#include "UnityStandardShadow.cginc"

					ENDCG
				}
					// ------------------------------------------------------------------
					//  Deferred pass
					Pass
					{
						Name "DEFERRED"
						Tags { "LightMode" = "Deferred" }

						CGPROGRAM
						#pragma target 3.0
					// TEMPORARY: GLES2.0 temporarily disabled to prevent errors spam on devices without textureCubeLodEXT
					#pragma exclude_renderers nomrt gles


					// -------------------------------------

					#pragma shader_feature _NORMALMAP
					#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
					#pragma shader_feature _EMISSION
					#pragma shader_feature _METALLICGLOSSMAP
					#pragma shader_feature ___ _DETAIL_MULX2
					#pragma shader_feature _PARALLAXMAP

					#pragma multi_compile ___ UNITY_HDR_ON
					#pragma multi_compile LIGHTMAP_OFF LIGHTMAP_ON
					#pragma multi_compile DIRLIGHTMAP_OFF DIRLIGHTMAP_COMBINED DIRLIGHTMAP_SEPARATE
					#pragma multi_compile DYNAMICLIGHTMAP_OFF DYNAMICLIGHTMAP_ON

					#pragma vertex vertDeferred
					#pragma fragment fragDeferred

					#include "UnityStandardCore.cginc"

					ENDCG
				}

					// ------------------------------------------------------------------
					// Extracts information for lightmapping, GI (emission, albedo, ...)
					// This pass it not used during regular rendering.
					Pass
					{
						Name "META"
						Tags { "LightMode" = "Meta" }

						Cull Off

						CGPROGRAM
						#pragma vertex vert_meta
						#pragma fragment frag_meta

						#pragma shader_feature _EMISSION
						#pragma shader_feature _METALLICGLOSSMAP
						#pragma shader_feature ___ _DETAIL_MULX2

						#include "UnityStandardMeta.cginc"
						ENDCG
					}
		}

			SubShader
				{
					Tags { "RenderType" = "Opaque" "PerformanceChecks" = "False" }
					LOD 150

					// ------------------------------------------------------------------
					//  Base forward pass (directional light, emission, lightmaps, ...)
					Pass
					{
						Name "FORWARD"
						Tags { "LightMode" = "ForwardBase" }

						Blend[_SrcBlend][_DstBlend]
						ZWrite[_ZWrite]

						CGPROGRAM
						#pragma target 2.0

						#pragma shader_feature _NORMALMAP
						#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
						#pragma shader_feature _EMISSION
						#pragma shader_feature _METALLICGLOSSMAP
						#pragma shader_feature ___ _DETAIL_MULX2
					// SM2.0: NOT SUPPORTED shader_feature _PARALLAXMAP

					#pragma skip_variants SHADOWS_SOFT DIRLIGHTMAP_COMBINED DIRLIGHTMAP_SEPARATE

					#pragma multi_compile_fwdbase
					#pragma multi_compile_fog

					#pragma vertex vertForwardBase
					#pragma fragment fragForwardBase

					#include "UnityStandardCore.cginc"

					ENDCG
				}
					// ------------------------------------------------------------------
					//  Additive forward pass (one light per pass)
					Pass
					{
						Name "FORWARD_DELTA"
						Tags { "LightMode" = "ForwardAdd" }
						Blend[_SrcBlend] One
						Fog { Color(0,0,0,0) } // in additive pass fog should be black
						ZWrite Off
						ZTest LEqual

						CGPROGRAM
						#pragma target 2.0

						#pragma shader_feature _NORMALMAP
						#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
						#pragma shader_feature _METALLICGLOSSMAP
						#pragma shader_feature ___ _DETAIL_MULX2
					// SM2.0: NOT SUPPORTED shader_feature _PARALLAXMAP
					#pragma skip_variants SHADOWS_SOFT

					#pragma multi_compile_fwdadd_fullshadows
					#pragma multi_compile_fog

					#pragma vertex vertForwardAdd
					#pragma fragment fragForwardAdd

					#include "UnityStandardCore.cginc"

					ENDCG
				}
					// ------------------------------------------------------------------
					//  Shadow rendering pass
					Pass {
						Name "ShadowCaster"
						Tags { "LightMode" = "ShadowCaster" }

						ZWrite On ZTest LEqual

						CGPROGRAM
						#pragma target 2.0

						#pragma shader_feature _ _ALPHATEST_ON _ALPHABLEND_ON _ALPHAPREMULTIPLY_ON
						#pragma skip_variants SHADOWS_SOFT
						#pragma multi_compile_shadowcaster

						#pragma vertex vertShadowCaster
						#pragma fragment fragShadowCaster

						#include "UnityStandardShadow.cginc"

						ENDCG
					}

					// ------------------------------------------------------------------
					// Extracts information for lightmapping, GI (emission, albedo, ...)
					// This pass it not used during regular rendering.
					Pass
					{
						Name "META"
						Tags { "LightMode" = "Meta" }

						Cull Off

						CGPROGRAM
						#pragma vertex vert_meta
						#pragma fragment frag_meta

						#pragma shader_feature _EMISSION
						#pragma shader_feature _METALLICGLOSSMAP
						#pragma shader_feature ___ _DETAIL_MULX2

						#include "UnityStandardMeta.cginc"
						ENDCG
					}
				}

					FallBack "Standard"
					CustomEditor "CustomStandardShaderGUI"
}
