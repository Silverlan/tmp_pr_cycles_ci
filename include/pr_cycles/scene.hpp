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
};
namespace OpenImageIO_v2_1
{
	class ustring;
};
namespace pragma
{
	class CAnimatedComponent;
	class CLightMapComponent;
	namespace physics {class Transform; class ScaledTransform;};
};
namespace util::bsp {struct LightMapInfo;};
namespace uimg {class ImageBuffer;};
class Model;
class ModelMesh;
class ModelSubMesh;
class Material;
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
		virtual void Cancel(const std::string &resultMsg) override;
		virtual void Wait() override;
		virtual std::shared_ptr<uimg::ImageBuffer> GetResult() override;
	private:
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
			SceneNormals
		};
		enum class StateFlags : uint16_t
		{
			None = 0u,
			DenoiseResult = 1u,
			OutputResultWithHDRColors = DenoiseResult<<1u,
			SkyInitialized = OutputResultWithHDRColors<<1u
		};
		static bool IsRenderSceneMode(RenderMode renderMode);
		static std::shared_ptr<Scene> Create(RenderMode renderMode,std::optional<uint32_t> sampleCount={},bool hdrOutput=false,bool denoise=true);
		//
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
			const std::function<bool(ModelMesh&)> &meshFilter=nullptr,const std::function<bool(ModelSubMesh&)> &subMeshFilter=nullptr
		);
		void AddSkybox(const std::string &texture);
		PMesh AddModel(
			BaseEntity &ent,Model &mdl,const std::string &meshName,uint32_t skinId=0,CAnimatedComponent *optAnimC=nullptr,
			const std::function<bool(ModelMesh&)> &optMeshFilter=nullptr,
			const std::function<bool(ModelSubMesh&)> &optSubMeshFilter=nullptr
		);
		void SetAOBakeTarget(BaseEntity &ent,Model &mdl,uint32_t matIndex);
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

		void SetSky(const std::string &skyPath);
		void SetSkyAngles(const EulerAngles &angSky);
		void SetSkyStrength(float strength);

		util::ParallelJob<std::shared_ptr<uimg::ImageBuffer>> Finalize();

		void AddShader(CCLShader &shader);
		ccl::Session *GetCCLSession();
	private:
		friend Shader;
		friend Object;
		friend Light;
		Scene(std::unique_ptr<ccl::Session> session,ccl::Scene &scene,RenderMode renderMode);
		static ccl::ShaderOutput *FindShaderNodeOutput(ccl::ShaderNode &node,const std::string &output);
		static ccl::ShaderNode *FindShaderNode(ccl::ShaderGraph &graph,const std::string &nodeName);
		static ccl::ShaderNode *FindShaderNode(ccl::ShaderGraph &graph,const OpenImageIO_v2_1::ustring &name);
		void InitializeAlbedoPass(bool reloadShaders);
		void InitializeNormalPass(bool reloadShaders);
		void ApplyPostProcessing(uimg::ImageBuffer &imgBuffer,cycles::Scene::RenderMode renderMode);
		void DenoiseHDRImageArea(uimg::ImageBuffer &imgBuffer,uint32_t imgWidth,uint32_t imgHeight,uint32_t x,uint32_t y,uint32_t w,uint32_t h) const;
		void AddMesh(BaseEntity &ent,Model &mdl,Mesh &mesh,ModelSubMesh &mdlMesh,pragma::CAnimatedComponent *optAnimC=nullptr,uint32_t skinId=0);
		void AddRoughnessMapImageTextureNode(ShaderModuleRoughness &shader,Material &mat,float defaultRoughness) const;
		PShader CreateShader(BaseEntity &ent,Mesh &mesh,Model &mdl,ModelSubMesh &subMesh,uint32_t skinId=0);
		Material *GetMaterial(Model &mdl,ModelSubMesh &subMesh,uint32_t skinId) const;
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

		void OnParallelWorkerCancelled();
		void Wait();
		friend SceneWorker;

		EulerAngles m_skyAngles = {};
		std::string m_sky = "";
		float m_skyStrength = 1.f;
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
