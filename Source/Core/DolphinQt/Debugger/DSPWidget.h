// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class QPlainTextEdit;
class QSplitter;
class QTableWidget;

namespace DSP
{
class DSPCore;
}
namespace Core
{
class System;
class CPUThreadGuard;
}

class DSPWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit DSPWidget(QWidget* parent = nullptr);
  ~DSPWidget();

private:
  void CreateWidgets();
  void Update();
  void UpdateAndShowPC();
  void OnRun();
  void OnStep();
  void OnIMemClick(int row, int column);
  void OnIMemChange(int row, int column);
  void OnRegChange(int row, int column);

  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

  DSP::DSPCore *GetDSPCore(const Core::CPUThreadGuard &);

  Core::System& m_system;

  int m_pc_row;
  QSplitter* m_splitter;
  QTableWidget* m_imem_table;
  QTableWidget* m_dmem_table;
  QTableWidget* m_regs_table;
};
