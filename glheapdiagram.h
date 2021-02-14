#ifndef GLHEAPDIAGRAM_H
#define GLHEAPDIAGRAM_H

#include <memory>

#include <QOpenGLWidget>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QPoint>

#include "activeregionsdiagramlayer.h"
#include "addressdiagramlayer.h"
#include "eventdiagramlayer.h"
#include "glheapdiagramlayer.h"
#include "heapblockdiagramlayer.h"
#include "heaphistory.h"
#include "transform3d.h"

class OpenGLShaderProgram;

class GLHeapDiagram : public QOpenGLWidget, protected QOpenGLFunctions {
public:
  explicit GLHeapDiagram(QWidget *parent = nullptr);
  ~GLHeapDiagram() override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

signals:
  void frameSwapped();
  void blockClicked(bool, HeapBlock);
  void showMessage(std::string);

public slots:
  void setFileToDisplay(const QString& filename);
  void setSizeToHighlight(uint32_t size);

protected slots:
  void update();

protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeGL(int width, int height) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  Q_OBJECT
  // This should be a power-of-2.
  static constexpr uint32_t number_of_grid_lines_ = 16;

  // Initialization of the GL members below.
  void setupHeapblockGLStructures();
  void setupGridGLStructures();

  void updateHeapToScreenMap();
  void updateUnitSquareToHeapMap();

  void getScaleFromHeapToScreen(double* scale_x, double* scale_y);
  void getScaleFromScreenToHeap(double* scale_x, double* scale_y);
  bool screenToHeap(double, double, uint32_t* tick, uint64_t* address);
  //void heapToScreen(uint32_t tick, uint64_t address, double*, double*);
  void loadFileInternal();

  void setHeapBaseUniforms();
  void setTickBaseUniforms();

  std::string file_to_load_;

  // Blocks of this size will be highlighted.
  uint32_t size_to_highlight_ = 0;
  bool refresh_all_vertices_ = false;

  // Gets set to true after the initializeGL() method runs.
  bool is_GL_initialized_;

  // Due to limited precision of floats, a straight mapping void setTickBaseUniforms();
  // heap to the screen space can have a degenerate matrix with a zero
  // entry. In order to prevent this, the scaling down is implemented
  // with two matrices that are applied iteratively.
  QMatrix2x2 heap_to_screen_matrix_;

  std::unique_ptr<HeapBlockDiagramLayer> block_layer_;
  std::unique_ptr<EventDiagramLayer> event_layer_;
  std::unique_ptr<AddressDiagramLayer> address_layer_;
  std::unique_ptr<ActiveRegionsDiagramLayer> pages_layer_;

  // The heap history.
  HeapHistory heap_history_;

  // Last mouse position for dragging and selecting.
  QPoint last_mouse_position_;
  void setupUniformsForShaders();
  void debugDumpVerticesAndMappings();
};

#endif // GLHEAPDIAGRAM_H
