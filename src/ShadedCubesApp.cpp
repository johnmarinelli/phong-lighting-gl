#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include <numeric>

using namespace ci;
using namespace ci::app;
using namespace std;

GLfloat cube_colors[] = {
  // front colors
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 1.0, 1.0,
  // back colors
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 1.0, 1.0,
};

static const GLushort cube_elements[] =
{
  0, 1, 2, // 0
  2, 1, 3, // 1
  2, 3, 4, // 2
  4, 3, 5, // 3
  4, 5, 6, // 4
  6, 5, 7, // 5
  6, 7, 0, // 6
  0, 7, 1, // 7
  6, 0, 2, // 8
  2, 4, 6, // 9
  7, 5, 3, // 10
  7, 3, 1  // 11
};

static const GLfloat cube_vertices[] =
{
  -1.0f, -1.0f, -1.0f, // 0
  -1.0f,  1.0f, -1.0f, // 1
  1.0f, -1.0f, -1.0f,  // 2
  1.0f,  1.0f, -1.0f,  // 3
  1.0f, -1.0f,  1.0f,  // 4
  1.0f,  1.0f,  1.0f,  // 5
  -1.0f, -1.0f,  1.0f, // 6
  -1.0f,  1.0f,  1.0f, // 7
};

// static storage for shared triangles, indexed by vertex
static const std::map<int, std::vector<int>> sharedTriangles =
{
  std::pair<int, std::vector<int>>{0, std::vector<int>{0,6,7,8}},
  std::pair<int, std::vector<int>>{1, std::vector<int>{0,1,7,11}},
  std::pair<int, std::vector<int>>{2, std::vector<int>{0,1,2,8,9}},
  std::pair<int, std::vector<int>>{3, std::vector<int>{1,2,3,10,11}},
  std::pair<int, std::vector<int>>{4, std::vector<int>{2,3,4,9}},
  std::pair<int, std::vector<int>>{5, std::vector<int>{3,4,5,10}},
  std::pair<int, std::vector<int>>{6, std::vector<int>{4,5,6,8,9}},
  std::pair<int, std::vector<int>>{7, std::vector<int>{5,6,7,10,11}}
};

std::vector<glm::vec3> triangleNormals;
std::vector<glm::vec3> normals;

GLfloat rawNormals[/*normals.size()*/24];

struct triangle {
  glm::vec3 v0, v1, v2;
  
  triangle(const glm::vec3& v_0, const glm::vec3& v_1, const glm::vec3& v_2) : v0(v_0), v1(v_1), v2(v_2) {}
};

glm::vec3 calculateTriangleNormal(const triangle& t) {
  auto e0 = t.v1 - t.v0;
  auto e1 = t.v2 - t.v0;
  auto e0xe1 = glm::cross(e1, e0);
  
  glm::vec3 n = glm::normalize(e0xe1);
  
  if (e0xe1[0] == 0 && e0xe1[1] == 0 && e0xe1[2] == 0) {
    // can't mathematically normalize a 0 vector
    // auto minFloat = std::numeric_limits<float>::min(); // this gives +Inf when normalized
    auto minFloat = 0.0001f;
    n = glm::normalize(glm::vec3{minFloat, minFloat, minFloat});
  }
  
  return n;
}

class ShadedCubesApp : public App {
  public:
	void setup() override;
  void keyDown(cinder::app::KeyEvent event) override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
  
  gl::GlslProgRef mGlsl;
  GLuint mCubeVertices, mCubeColors, mCubeNormals, mCubeIBO, mVao;
  
  struct {
    GLuint model_matrix;
    GLuint proj_matrix;
    GLuint view_matrix;
    GLuint mv_matrix;
  } mUniforms;

};

void ShadedCubesApp::setup()
{
  mGlsl = gl::GlslProg::create(loadResource("shadedcubes.vert"), loadResource("shadedcubes.frag"));
  
  mUniforms.model_matrix = glGetUniformLocation(mGlsl->getHandle(), "model_matrix");
  mUniforms.proj_matrix = glGetUniformLocation(mGlsl->getHandle(), "proj_matrix");
  mUniforms.view_matrix = glGetUniformLocation(mGlsl->getHandle(), "view_matrix");
  mUniforms.mv_matrix = glGetUniformLocation(mGlsl->getHandle(), "mv_matrix");
  
  glGenVertexArrays(1, &mVao);
  glBindVertexArray(mVao);
  
  glGenBuffers(1, &mCubeVertices);
  glBindBuffer(GL_ARRAY_BUFFER, mCubeVertices);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  
  glGenBuffers(1, &mCubeIBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mCubeIBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_elements), cube_elements, GL_STATIC_DRAW);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  
  int indexScale = 3;
  for (auto i = 0; i < 12; ++i) {
    auto vi = i * indexScale;
    
    // get indices to reference cube_vertices
    auto i0 = cube_elements[vi];
    auto i1 = cube_elements[vi+1];
    auto i2 = cube_elements[vi+2];
    
    // scale cube_vertices indices
    i0 *= indexScale;
    i1 *= indexScale;
    i2 *= indexScale;
    
    glm::vec3 p0{cube_vertices[i0], cube_vertices[i0 + 1], cube_vertices[i0 + 2]};
    glm::vec3 p1{cube_vertices[i1], cube_vertices[i1 + 1], cube_vertices[i0 + 2]};
    glm::vec3 p2{cube_vertices[i2], cube_vertices[i2 + 1], cube_vertices[i2 + 2]};

    auto t = triangle(p0, p1, p2);
    auto tn = calculateTriangleNormal(t);
    triangleNormals.push_back(tn);
  }
  
  // make a copy so we can capture in std::accumulate's lambda below
  auto tns = triangleNormals;
  
  for (const auto& kv : sharedTriangles) {
    // triangle indices for vector i
    auto tis = kv.second;
    
    // pick up indices from tn
    auto vn = std::accumulate(tis.begin(), tis.end(), glm::vec3{0.f}, [tns] (glm::vec3 n, int idx) {
      auto tn = tns[idx];
      return n + tn;
    });
    
    auto vnn = glm::normalize(vn);
    normals.push_back(vnn);
  }
  
  
  for (auto i = 0; i < normals.size(); ++i) {
    auto v = normals[i];
    auto x = v.x;
    auto y = v.y;
    auto z = v.z;
    
    auto ii = i * indexScale;
    
    rawNormals[ii] = x;
    rawNormals[ii+1] = y;
    rawNormals[ii+2] = z;
  }
  
  glGenBuffers(1, &mCubeNormals);
  glBindBuffer(GL_ARRAY_BUFFER, mCubeNormals);
  glBufferData(GL_ARRAY_BUFFER, sizeof(rawNormals), rawNormals, GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);

  glGenBuffers(1, &mCubeColors);
  glBindBuffer(GL_ARRAY_BUFFER, mCubeColors);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cube_colors), cube_colors, GL_STATIC_DRAW);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(2);
  
}

void ShadedCubesApp::keyDown(cinder::app::KeyEvent event)
{
  auto input = event.getChar();
}

void ShadedCubesApp::mouseDown( MouseEvent event )
{
}

void ShadedCubesApp::update()
{
}

void ShadedCubesApp::draw()
{
  static const GLfloat one = 1.f;
  static const GLfloat bg[] = {0.f, 0.0f, 0.f, 1.f};
  
  glViewport(0, 0, getWindowWidth(), getWindowHeight());
  glClearBufferfv(GL_COLOR, 0, bg);
  glClearBufferfv(GL_DEPTH, 0, &one);
  
  mGlsl->bind();
  
  glm::mat4 projMatrix = glm::perspective(50.f, getWindowAspectRatio(), 0.1f, 1000.f);
  glUniformMatrix4fv(mUniforms.proj_matrix, 1, GL_FALSE, glm::value_ptr(projMatrix));
  auto seconds = getElapsedSeconds() * 0.01;
  float f = seconds * 0.03;

  auto viewMatrix = glm::translate(glm::mat4{}, glm::vec3{0.f, 0.f, -100.f});
  glUniformMatrix4fv(mUniforms.view_matrix, 1, GL_FALSE, glm::value_ptr(viewMatrix));
  
  auto translationMatrix = glm::translate(glm::mat4{},
                                          glm::vec3{sinf(2.1f * f) * 0.5f,
                                            cosf(1.7f * f) * 0.5f,
                                            sinf(1.3f * f) * cosf(1.5f * f) * 2.f});
  
  auto rotationMatrix = glm::rotate(glm::mat4{}, (float)seconds * 45.f, glm::vec3{0.f, 1.f, 0.f});// *
    //glm::rotate(glm::mat4{}, (float)seconds * 81.f, glm::vec3{1.f, 0.f, 0.f});
  
  auto scaleMatrix = glm::scale(glm::mat4(), glm::vec3(2.f));
  
  auto modelMatrix = translationMatrix * rotationMatrix * scaleMatrix * glm::mat4{};
  glUniformMatrix4fv(mUniforms.model_matrix, 1, GL_FALSE, glm::value_ptr(modelMatrix));
  
  auto mvMatrix = viewMatrix * modelMatrix;  
  glUniformMatrix4fv(mUniforms.mv_matrix, 1, GL_FALSE, glm::value_ptr(mvMatrix));
  
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
}

CINDER_APP( ShadedCubesApp, RendererGl )
