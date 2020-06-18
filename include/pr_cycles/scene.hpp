#ifndef __PR_CYCLES_SCENE_HPP__
#define __PR_CYCLES_SCENE_HPP__

#include <sharedutils/util_weak_handle.hpp>
#include <sharedutils/util_parallel_job.hpp>
#include "nodes.hpp"
#include <memory>
#include <mathutil/uvec.h>
#include <functional>
#include <optional>
#include <atomic>
#include <thread>

#define ENABLE_TEST_AMBIENT_OCCLUSION

class BaseEntity;

namespace ccl
{
	class Session;
	class Scene;
	class ShaderInput;
	class ShaderNode;
	class ShaderOutput;
	class ShaderGraph;
	struct float3;
	struct float2;
	struct Transform;
	class ImageTextureNode;
	class EnvironmentTextureNode;
	class BufferParams;
	class SessionParams;
};
namespace OpenImageIO_v2_1
{
	class ustring;
};
namespace pragma
{
	class CAnimatedComponent;
	class CLightMapComponent;
	class CParticleSystemComponent;
	class CSkyCameraComponent;
	class CModelComponent;
	namespace physics {class Transform; class ScaledTransform;};
};
namespace util::bsp {struct LightMapInfo;};
namespace uimg {class ImageBuffer;};
class Model;
class ModelMesh;
class ModelSubMesh;
class Material;
class CParticle;
namespace pragma::modules::cycles
{
	class SceneObject;
	class Scene;
	class Shader;
	class ShaderModuleRoughness;
	using PShader = std::shared_ptr<Shader>;
	class Object;
	using PObject = std::shared_ptr<Object>;
	class Light;
	using PLight = std::shared_ptr<Light>;
	class Camera;
	using PCamera = std::shared_ptr<Camera>;
	using PScene = std::shared_ptr<Scene>;
	class Mesh;
	using PMesh = std::shared_ptr<Mesh>;
	struct Socket;

	class SceneWorker
		: public util::ParallelWorker<std::shared_ptr<uimg::ImageBuffer>>
	{
	public:
		friend Scene;
		SceneWorker(Scene &scene);
		using util::ParallelWorker<std::shared_ptr<uimg::ImageBuffer>>::Cancel;
		virtual void Wait() override;
		virtual std::shared_ptr<uimg::ImageBuffer> GetResult() override;
	private:
		virtual void DoCancel(const std::string &resultMsg) override;
		PScene m_scene = nullptr;
		template<typename TJob,typename... TARGS>
			friend util::ParallelJob<typename TJob::RESULT_TYPE> util::create_parallel_job(TARGS&& ...args);
	};

	class Scene
		: public std::enable_shared_from_this<Scene>
	{
	public:
		struct DenoiseInfo
		{
			uint32_t numThreads = 16;
			uint32_t width = 0;
			uint32_t height = 0;
			bool hdr = false;
			bool lightmap = false;
		};
		enum class ColorSpace : uint8_t
		{
			SRGB = 0,
			Raw
		};
		enum class RenderMode : uint8_t
		{
			RenderImage = 0u,
			BakeAmbientOcclusion,
			BakeNormals,
			BakeDiffuseLighting,
			SceneAlbedo,
			SceneNormals,
			SceneDepth
		};
		enum class StateFlags : uint16_t
		{
			None = 0u,
			DenoiseResult = 1u,
			OutputResultWithHDRColors = DenoiseResult<<1u,
			SkyInitialized = OutputResultWithHDRColors<<1u
		};
		enum class DeviceType : uint8_t
		{
			CPU = 0u,
			GPU,

			Count
		};

		struct CreateInfo
		{
			std::optional<uint32_t> samples = {};
			bool hdrOutput = false;
			bool denoise = true;
			DeviceType deviceType = DeviceType::GPU;
		};
		static bool IsRenderSceneMode(RenderMode renderMode);
		static std::shared_ptr<Scene> Create(RenderMode renderMode,const CreateInfo &createInfo={});
		//
		static Vector3 ToPragmaPosition(const ccl::float3 &pos);
		static ccl::float3 ToCyclesVector(const Vector3 &v);
		static ccl::float3 ToCyclesPosition(const Vector3 &pos);
		static ccl::float3 ToCyclesNormal(const Vector3 &n);
		static ccl::float2 ToCyclesUV(const Vector2 &uv);
		static ccl::Transform ToCyclesTransform(const pragma::physics::ScaledTransform &t);
		static float ToCyclesLength(float len);
		static bool Denoise(
			const DenoiseInfo &denoise,float *inOutData,
			float *optAlbedoData=nullptr,float *optInNormalData=nullptr,
			const std::function<bool(float)> &fProgressCallback=nullptr
		);

		static constexpr uint32_t INPUT_CHANNEL_COUNT = 4u;
		static constexpr uint32_t OUTPUT_CHANNEL_COUNT = 4u;

		~Scene();
		PObject AddEntity(
			BaseEntity &ent,std::vector<ModelSubMesh*> *optOutTargetMeshes=nullptr,
			const std::function<bool(ModelMesh&,const Vector3&,const Quat&)> &meshFilter=nullptr,const std::function<bool(ModelSubMesh&,const Vector3&,const Quat&)> &subMeshFilter=nullptr,
			const std::string &nameSuffix=""
		);
		void AddParticleSystem(pragma::CParticleSystemComponent &ptc,const Vector3 &camPos,const Mat4 &vp,float nearZ,float farZ);
		void AddSkybox(const std::string &texture);
		PMesh AddModel(
			Model &mdl,const std::string &meshName,BaseEntity *optEnt=nullptr,uint32_t skinId=0,
			CModelComponent *optMdlC=nullptr,CAnimatedComponent *optAnimC=nullptr,
			const std::function<bool(ModelMesh&,const Vector3&,const Quat&)> &optMeshFilter=nullptr,
			const std::function<bool(ModelSubMesh&,const Vector3&,const Quat&)> &optSubMeshFilter=nullptr
		);
		PMesh AddMeshList(
			Model &mdl,const std::vector<std::shared_ptr<ModelMesh>> &meshList,const std::string &meshName,BaseEntity *optEnt=nullptr,uint32_t skinId=0,
			CModelComponent *optMdlC=nullptr,CAnimatedComponent *optAnimC=nullptr,
			const std::function<bool(ModelMesh&,const Vector3&,const Quat&)> &optMeshFilter=nullptr,
			const std::function<bool(ModelSubMesh&,const Vector3&,const Quat&)> &optSubMeshFilter=nullptr
		);
		void Add3DSkybox(pragma::CSkyCameraComponent &skyCam,const Vector3 &camPos);
		void SetAOBakeTarget(Model &mdl,uint32_t matIndex);
		void SetLightmapBakeTarget(BaseEntity &ent);
		Camera &GetCamera();
		float GetProgress() const;
		RenderMode GetRenderMode() const;
		ccl::Scene *operator->();
		ccl::Scene *operator*();

		const std::vector<PShader> &GetShaders() const;
		std::vector<PShader> &GetShaders();
		const std::vector<PObject> &GetObjects() const;
		std::vector<PObject> &GetObjects();
		const std::vector<PLight> &GetLights() const;
		std::vector<PLight> &GetLights();

		void SetLightIntensityFactor(float f);
		float GetLightIntensityFactor() const;

		void SetSky(const std::string &skyPath);
		void SetSkyAngles(const EulerAngles &angSky);
		void SetSkyStrength(float strength);
		void SetEmissionStrength(float strength);
		void SetMaxTransparencyBounces(uint32_t maxBounces);
		void SetMotionBlurStrength(float strength);

		util::ParallelJob<std::shared_ptr<uimg::ImageBuffer>> Finalize();

		void AddShader(CCLShader &shader);
		ccl::Session *GetCCLSession();
	private:
		struct ShaderInfo
		{
			// These are only required if the shader is used for eyeballs
			std::optional<BaseEntity*> entity = {};
			std::optional<ModelSubMesh*> subMesh = {};

			std::optional<pragma::CParticleSystemComponent*> particleSystem = {};
			std::optional<const void*> particle = {};
		};
		friend Shader;
		friend Object;
		friend Light;
		Scene(std::unique_ptr<ccl::Session> session,ccl::Scene &scene,RenderMode renderMode,DeviceType deviceType);
		static ccl::ShaderOutput *FindShaderNodeOutput(ccl::ShaderNode &node,const std::string &output);
		static ccl::ShaderNode *FindShaderNode(ccl::ShaderGraph &graph,const std::string &nodeName);
		static ccl::ShaderNode *FindShaderNode(ccl::ShaderGraph &graph,const OpenImageIO_v2_1::ustring &name);
		void InitializeAlbedoPass(bool reloadShaders);
		void InitializeNormalPass(bool reloadShaders);
		void ApplyPostProcessing(uimg::ImageBuffer &imgBuffer,cycles::Scene::RenderMode renderMode);
		void DenoiseHDRImageArea(uimg::ImageBuffer &imgBuffer,uint32_t imgWidth,uint32_t imgHeight,uint32_t x,uint32_t y,uint32_t w,uint32_t h) const;
		void AddMesh(Model &mdl,Mesh &mesh,ModelSubMesh &mdlMesh,pragma::CModelComponent *optMdlC=nullptr,pragma::CAnimatedComponent *optAnimC=nullptr,BaseEntity *optEnt=nullptr,uint32_t skinId=0);
		void AddRoughnessMapImageTextureNode(ShaderModuleRoughness &shader,Material &mat,float defaultRoughness) const;
		PShader CreateShader(Material &mat,const std::string &meshName,const ShaderInfo &shaderInfo={});
		PShader CreateShader(Mesh &mesh,Model &mdl,ModelSubMesh &subMesh,BaseEntity *optEnt=nullptr,uint32_t skinId=0);
		Material *GetMaterial(Model &mdl,ModelSubMesh &subMesh,uint32_t skinId) const;
		Material *GetMaterial(pragma::CModelComponent &mdlC,ModelSubMesh &subMesh,uint32_t skinId) const;
		Material *GetMaterial(BaseEntity &ent,ModelSubMesh &subMesh,uint32_t skinId) const;
		bool Denoise(
			const DenoiseInfo &denoise,uimg::ImageBuffer &imgBuffer,
			uimg::ImageBuffer *optImgBufferAlbedo=nullptr,
			uimg::ImageBuffer *optImgBufferNormal=nullptr,
			const std::function<bool(float)> &fProgressCallback=nullptr
		) const;
		bool IsValidTexture(const std::string &filePath) const;
		void CloseCyclesScene();
		void FinalizeAndCloseCyclesScene();
		std::shared_ptr<uimg::ImageBuffer> FinalizeCyclesScene();
		ccl::BufferParams GetBufferParameters() const;

		void SetupRenderSettings(
			ccl::Scene &scene,ccl::Session &session,ccl::BufferParams &bufferParams,cycles::Scene::RenderMode renderMode,
			uint32_t maxTransparencyBounces
		) const;

		void OnParallelWorkerCancelled();
		void Wait();
		friend SceneWorker;

		EulerAngles m_skyAngles = {};
		std::string m_sky = "";
		float m_skyStrength = 1.f;
		float m_emissionStrength = 1.f;
		float m_lightIntensityFactor = 1.f;
		float m_motionBlurStrength = 0.f;
		uint32_t m_maxTransparencyBounces = 64;
		DeviceType m_deviceType = DeviceType::GPU;
		std::vector<PShader> m_shaders = {};
		std::vector<std::shared_ptr<CCLShader>> m_cclShaders = {};
		std::vector<PObject> m_objects = {};
		std::vector<PLight> m_lights = {};
		std::unique_ptr<ccl::Session> m_session = nullptr;
		ccl::Scene &m_scene;
		PCamera m_camera = nullptr;
		StateFlags m_stateFlags = StateFlags::None;
		RenderMode m_renderMode = RenderMode::RenderImage;
		std::weak_ptr<Object> m_bakeTarget = {};
		util::WeakHandle<pragma::CLightMapComponent> m_lightmapTargetComponent = {};
		std::shared_ptr<uimg::ImageBuffer> m_resultImageBuffer = nullptr;
		std::shared_ptr<uimg::ImageBuffer> m_normalImageBuffer = nullptr;
		std::shared_ptr<uimg::ImageBuffer> m_albedoImageBuffer = nullptr;
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(pragma::modules::cycles::Scene::StateFlags)

#endif
