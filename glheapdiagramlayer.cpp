// A "layer" in the heap diagram - the actual diagram for the user is composed
// of several such layers (heap blocks, horizontal lines ~ 'addresses', vertical
// lines ~ 'events). Since each layer has to perform extremely similar actions
// but call different shaders with different geometry, most of the base case is
// handled in this class.

#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>

#include <cinttypes>
#include <utility>

#include "glheapdiagramlayer.h"

// Users of this class need to provide a vertex and fragment shader program, and
// indicate if the layer is a lines-only layer or of actual triangles / rectangles
// ought to be drawn.
GLHeapDiagramLayer::GLHeapDiagramLayer(
    std::string vertex_shader_name,
    std::string fragment_shader_name, bool is_line_layer)
    : vertex_shader_name_(std::move(vertex_shader_name)),
      fragment_shader_name_(std::move(fragment_shader_name)),
      is_line_layer_(is_line_layer),
      layer_shader_program_(new QOpenGLShaderProgram()) {}

GLHeapDiagramLayer::~GLHeapDiagramLayer() {
  if (is_initialized_) {
    layer_vao_.destroy();
    layer_vertex_buffer_.destroy();
  }
}

void GLHeapDiagramLayer::refreshGLBuffer(bool bind) {
  if (bind) {
    layer_vertex_buffer_.bind();
  }

  int needed_size = static_cast<int>((layer_vertices_.size() * sizeof(HeapVertex)));
  if (layer_vertex_buffer_.size() < needed_size) {
    layer_vertex_buffer_.allocate(needed_size);
  }
  if(!layer_vertices_.empty()) {
      layer_vertex_buffer_.write(0, &layer_vertices_[0],
                                 needed_size);
  }
  if (bind) {
    layer_vertex_buffer_.release();
  }
}

void GLHeapDiagramLayer::refreshVertices(const HeapHistory& heap_history, bool bind, bool all) {
  loadVerticesFromHeapHistory(heap_history, all);
  refreshGLBuffer(bind);
}

// Mostly boilerplate code for OpenGL -- load the shaders, link and bind them,
// create a vertex etc.
void GLHeapDiagramLayer::initializeGLStructures(
  const HeapHistory& heap_history, QOpenGLFunctions *parent) {
  if (!is_initialized_) {
    // Load the shaders.
    layer_shader_program_->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                                 vertex_shader_name_.c_str());
    layer_shader_program_->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                                 fragment_shader_name_.c_str());
    layer_shader_program_->link();
    layer_shader_program_->bind();

    // Create the heap block vertex buffer.
    layer_vertex_buffer_.create();
    layer_vertex_buffer_.bind();
    layer_vertex_buffer_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
  }

  refreshVertices(heap_history, false);

  if (!is_initialized_) {
    setupStandardUniforms();

    // Create the vertex array object.
    layer_vao_.create();
    layer_vao_.bind();

    // Register attribute arrays with stride for the vertex attributes.
    layer_shader_program_->enableAttributeArray(0);
    layer_shader_program_->enableAttributeArray(1);
    layer_shader_program_->setAttributeBuffer(
        0, GL_FLOAT, HeapVertex::positionOffset(), HeapVertex::PositionTupleSize,
        HeapVertex::stride());
    layer_shader_program_->setAttributeBuffer(
        1, GL_FLOAT, HeapVertex::colorOffset(), HeapVertex::ColorTupleSize,
        HeapVertex::stride());
    parent->glBindAttribLocation(layer_shader_program_->programId(), 0,
                                 "position");
    parent->glBindAttribLocation(layer_shader_program_->programId(), 1, "color");

    // Unbind.
    layer_vao_.release();
    layer_vertex_buffer_.release();
    layer_shader_program_->release();
  }
  is_initialized_ = true;
}

void GLHeapDiagramLayer::setupStandardUniforms() {
  uniform_vertex_to_screen_ =
      layer_shader_program_->uniformLocation("scale_heap_to_screen");
  // The reason why three numbers are needed for the visible heap base
  // is that the central heap rectangle can in theory range from
  // 0 to 2^64-1. This means that the minimum address that can be on
  // screen can be as low as -2^63 (e.g. half a maximum-sized heap down)
  // or as high as 2^64+2^63.
  uniform_visible_heap_base_A_ =
      layer_shader_program_->uniformLocation("visible_heap_base_A");
  uniform_visible_heap_base_B_ =
      layer_shader_program_->uniformLocation("visible_heap_base_B");
  uniform_visible_heap_base_C_ =
      layer_shader_program_->uniformLocation("visible_heap_base_C");
  // The lowest visible tick is passed as 64 bits (e.g. two 32-bit numbers).
  uniform_visible_tick_base_A_ =
      layer_shader_program_->uniformLocation("visible_tick_base_A");
  uniform_visible_tick_base_B_ =
      layer_shader_program_->uniformLocation("visible_tick_base_B");
}

// Set the heap base.
void GLHeapDiagramLayer::setHeapBaseUniforms(int32_t x, int32_t y, int32_t z) {
  visible_heap_base_A_ = x;
  visible_heap_base_B_ = y;
  visible_heap_base_C_ = z;
  layer_shader_program_->setUniformValue(uniform_visible_heap_base_A_, x);
  layer_shader_program_->setUniformValue(uniform_visible_heap_base_B_, y);
  layer_shader_program_->setUniformValue(uniform_visible_heap_base_C_, z);
}

// Sends the value of the matrix that performs heap-space-to-screen-space
// transformation to the shader.
void GLHeapDiagramLayer::setHeapToScreenMatrix(
    const QMatrix2x2 &heap_to_screen) {
  vertex_to_screen_ = heap_to_screen;
  layer_shader_program_->setUniformValue(uniform_vertex_to_screen_,
                                         heap_to_screen);
}

void GLHeapDiagramLayer::paintLayer(ivec2 tick, ivec3 address,
                                    const QMatrix2x2 &heap_to_screen) {
  layer_shader_program_->bind();
  setHeapToScreenMatrix(heap_to_screen);
  setHeapBaseUniforms(address.x, address.y, address.z);
  setTickBaseUniforms(tick.x, tick.y);

  {
    layer_vao_.bind();
    auto count = static_cast<uint32_t>((layer_vertices_.size() * sizeof(HeapVertex)));
    glDrawArrays(is_line_layer_ ? GL_LINES : GL_TRIANGLES, 0, count);
    layer_vao_.release();
  }
  layer_shader_program_->release();
}

void GLHeapDiagramLayer::setTickBaseUniforms(int32_t x, int32_t y) {
  visible_tick_base_A_ = x;
  visible_tick_base_B_ = y;
  layer_shader_program_->setUniformValue(uniform_visible_tick_base_A_, x);
  layer_shader_program_->setUniformValue(uniform_visible_tick_base_B_, y);
}

void GLHeapDiagramLayer::debugDumpVertexTransformation() {
  printf("[Debug] Start of layer dump (vertex shader '%s').\n",
    vertex_shader_name_.c_str());
  for (size_t index = 0; index < layer_vertices_.size(); ++index) {
    const HeapVertex& vertex = layer_vertices_[index];
    std::pair<vec4, vec4> result = vertexShaderSimulator(vertex);
    printf("%08x.%08x.%08x (%d, %" PRIx64 ", Color %f.%f.%f) --> ", visible_heap_base_C_, visible_heap_base_B_, visible_heap_base_A_, vertex.getX(), vertex.getY(),
      vertex.getColor().x(), vertex.getColor().y(), vertex.getColor().z());
    printf("(%f, %f, %f, %f), (%f, %f, %f, %f)\n", result.first.w_, result.first.x_,
      result.first.y_, result.first.z_, result.second.w_, result.second.x_,
      result.second.y_, result.second.z_);
    if (is_line_layer_) {
      if ((index % 2) == 1) {
        printf("\n");
        fflush(stdout);
      }
    } else if ((index % 3) == 2) {
      printf("\n");
      fflush(stdout);
    }
  }
  printf("[Debug] End of layer dump.\n");
}
