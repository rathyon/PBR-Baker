#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>
#include <gli/gli.hpp>

#include <Shader.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

//const std::string hdrImagePath = "forest_cave_2k.hdr";

#define ENVMAP_RES 512
#define IRRADIANCEMAP_RES 32
#define PREFILTERMAP_RES 128
#define MAXMIPLEVELS 5

void renderQuad();
void renderCube();

static void error_callback(int error, const char* description) {
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void generateMaps(std::string filepath) {
    fs::path p = fs::path(filepath);
    fs::path savefolder = fs::path(p.parent_path().parent_path().string() + "/output/" + p.stem().string());
    if (!fs::exists(savefolder)) {
        fs::create_directory(savefolder);
    }

	// Load shaders...
    ShaderSource convolutionVS = ShaderSource(GL_VERTEX_SHADER, "shaders/convolution.vs");
    ShaderSource eqFS = ShaderSource(GL_FRAGMENT_SHADER, "shaders/equirectangular.fs");
    Shader equirectangularToCubemapShdr = Shader("equirectangularToCubemapShdr");
    equirectangularToCubemapShdr.addShader(convolutionVS);
    equirectangularToCubemapShdr.addShader(eqFS);
    equirectangularToCubemapShdr.link();

    ShaderSource irrFS = ShaderSource(GL_FRAGMENT_SHADER, "shaders/irradiance.fs");
    Shader irradianceShdr = Shader("irradianceShdr");
    irradianceShdr.addShader(convolutionVS);
    irradianceShdr.addShader(irrFS);
    irradianceShdr.link();

    ShaderSource prefFS = ShaderSource(GL_FRAGMENT_SHADER, "shaders/prefilter.fs");
    Shader prefilterShdr = Shader("prefilterShdr");
    prefilterShdr.addShader(convolutionVS);
    prefilterShdr.addShader(prefFS);
    prefilterShdr.link();

	// Setup framebuffer
    unsigned int captureFBO;
    unsigned int captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, ENVMAP_RES, ENVMAP_RES);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Load image
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float* data = stbi_loadf(filepath.c_str(), &width, &height, &nrComponents, 0);
    unsigned int hdrTexture;
    if (data)
    {
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image: " << stbi_failure_reason() << std::endl;
        exit(EXIT_FAILURE);
    }

	// Setup cubemap
    unsigned int envCubemap;
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, ENVMAP_RES, ENVMAP_RES, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // enable pre-filter mipmap sampling (combatting visible dots artifact)
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Setup view + proj matrices
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

    // Convert Equirectangular to Cubemap
    glUseProgram(equirectangularToCubemapShdr.id());
    glUniform1i(glGetUniformLocation(equirectangularToCubemapShdr.id(), "equirectangularMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(equirectangularToCubemapShdr.id(), "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, ENVMAP_RES, ENVMAP_RES); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(equirectangularToCubemapShdr.id(), "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    // Store cubemap into .dds file
    gli::texture_cube envCubeMapDDS = gli::texture_cube(gli::FORMAT_RGB16_SFLOAT_PACK16, gli::extent2d(ENVMAP_RES, ENVMAP_RES));
    for (int face = 0; face < 6; face++) {
        float* texData = new float[3 * ENVMAP_RES * ENVMAP_RES];
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB, GL_FLOAT, texData);
        for (int y = 0; y < ENVMAP_RES; y++) {
            for (int x = 0; x < ENVMAP_RES; x++) {
                unsigned int row = y * ENVMAP_RES * 3;
                unsigned int col = x * 3;
                glm::vec3 texelData = glm::vec3(texData[row + col], texData[row + col + 1], texData[row + col + 2]);
                envCubeMapDDS.store<glm::highp_u16vec3>({ x, y }, face, 0, gli::packHalf(texelData));

                //envCubeMapDDS.store<glm::highp_u16vec3>({ y, ENVMAP_RES - 1 - x }, face, 0, gli::packHalf(glm::vec3(texData[y + x], texData[y + x + 1], texData[y + x + 2])));
            }
        }
        delete[] texData;
    }

    if (!gli::save(envCubeMapDDS, savefolder.string() + "/" + "env.dds")) {
        std::cout << "[ERROR] Failed to save environment cubemap!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Environment Cubemap saved at: " << savefolder.string() + "/" + "env.dds" << std::endl;

    /**/
    // Init Irradiance cubemap
    unsigned int irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, IRRADIANCEMAP_RES, IRRADIANCEMAP_RES, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IRRADIANCEMAP_RES, IRRADIANCEMAP_RES);

    // Generate irradiance data
    glUseProgram(irradianceShdr.id());
    glUniform1i(glGetUniformLocation(irradianceShdr.id(), "environmentMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(irradianceShdr.id(), "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glViewport(0, 0, IRRADIANCEMAP_RES, IRRADIANCEMAP_RES); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(irradianceShdr.id(), "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Store cubemap into .dds file
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    gli::texture_cube irradianceMapDDS = gli::texture_cube(gli::FORMAT_RGB16_SFLOAT_PACK16, gli::extent2d(IRRADIANCEMAP_RES, IRRADIANCEMAP_RES));
    for (int face = 0; face < 6; face++) {
        float* texData = new float[3 * IRRADIANCEMAP_RES * IRRADIANCEMAP_RES];
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB, GL_FLOAT, texData);
        for (int y = 0; y < IRRADIANCEMAP_RES; y++) {
            for (int x = 0; x < IRRADIANCEMAP_RES; x++) {
                unsigned int row = y * IRRADIANCEMAP_RES * 3;
                unsigned int col = x * 3;
                glm::vec3 texelData = glm::vec3(texData[row + col], texData[row + col + 1], texData[row + col + 2]);
                irradianceMapDDS.store<glm::highp_u16vec3>({ x, y }, face, 0, gli::packHalf(texelData));

            }
        }
        delete[] texData;
    }

    if (!gli::save(irradianceMapDDS, savefolder.string() + "/" + "irradiance.dds")) {
        std::cout << "[ERROR] Failed to save irradiance cubemap!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Irradiance Cubemap saved at: " << savefolder.string() + "/" + "irradiance.dds" << std::endl;

    // Init prefilter Cubemap
    unsigned int prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, PREFILTERMAP_RES, PREFILTERMAP_RES, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Generate prefilter cubemap
    glUseProgram(prefilterShdr.id());
    glUniform1i(glGetUniformLocation(prefilterShdr.id(), "environmentMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(prefilterShdr.id(), "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int mip = 0; mip < MAXMIPLEVELS; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = PREFILTERMAP_RES * std::pow(0.5, mip);
        unsigned int mipHeight = PREFILTERMAP_RES * std::pow(0.5, mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(MAXMIPLEVELS - 1);
        glUniform1f(glGetUniformLocation(prefilterShdr.id(), "roughness"), roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glUniformMatrix4fv(glGetUniformLocation(prefilterShdr.id(), "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Store cubemap into .dds file
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    gli::texture_cube prefilterMapDDS = gli::texture_cube(gli::FORMAT_RGB16_SFLOAT_PACK16, gli::extent2d(PREFILTERMAP_RES, PREFILTERMAP_RES), MAXMIPLEVELS);

    for (unsigned int mip = 0; mip < MAXMIPLEVELS; ++mip) {
        for (int face = 0; face < 6; face++) {
            unsigned int mipRes = PREFILTERMAP_RES * std::pow(0.5, mip);
            float* texData = new float[3 * mipRes * mipRes];
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, GL_RGB, GL_FLOAT, texData);
            for (int y = 0; y < mipRes; y++) {
                for (int x = 0; x < mipRes; x++) {
                    unsigned int row = y * mipRes * 3;
                    unsigned int col = x * 3;
                    glm::vec3 texelData = glm::vec3(texData[row + col], texData[row + col + 1], texData[row + col + 2]);
                    prefilterMapDDS.store<glm::highp_u16vec3>({ x, y }, face, mip, gli::packHalf(texelData));

                }
            }
            delete[] texData;
        }
    }

    if (!gli::save(prefilterMapDDS, savefolder.string() + "/" + "ggx.dds")) {
        std::cout << "[ERROR] Failed to save prefilter cubemap!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Prefilter Cubemap saved at: " << savefolder.string() + "/" + "ggx.dds" << std::endl;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube() {
    // initialize (if necessary)
    if (cubeVAO == 0) {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
            1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
                                                                  // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
            1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                                                                // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
            1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
            1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
            1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
            1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
            1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
            1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
                                                                // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
            1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
            1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left                                                                                                                                                 
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

int main(int argc, char* argv[]) {
	int width, height;
    width = height = 512;


	glfwSetErrorCallback(error_callback);
	// Initialize GLFW
	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Enable MSAA
	glfwWindowHint(GLFW_SAMPLES, 4);

	GLFWwindow* window = glfwCreateWindow(width, height, "PBR Baker", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//Initialize GLEW
	glfwSetKeyCallback(window, key_callback);
	glfwMakeContextCurrent(window);
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		std::cerr << "ERROR: " << glewGetErrorString(err) << std::endl;
	}

	// Initialize OpenGL state
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	int fwidth, fheight;
	glfwGetFramebufferSize(window, &fwidth, &fheight);
	glViewport(0, 0, fwidth, fheight);

    std::string path = std::string(fs::current_path().string()) + "/input";
    for (const auto& entry : fs::directory_iterator(path)) {
        generateMaps(entry.path().string());
    }

    /** /
	while (!glfwWindowShouldClose(window)) {
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
    /**/

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}