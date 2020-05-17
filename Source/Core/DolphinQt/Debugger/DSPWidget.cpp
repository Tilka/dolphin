// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/DSPWidget.h"

#include <fmt/format.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Common/MemoryUtil.h"
#include "Core/Core.h"
#include "Core/DSPEmulator.h"
#include "Core/DSP/DSPAssembler.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPTables.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPLLE/DSPLLE.h"
#include "Core/HW/DSPLLE/DSPSymbols.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

DSPWidget::DSPWidget(QWidget* parent) : QDockWidget(parent), m_system(Core::System::GetInstance())
{
  setWindowTitle(tr("DSP"));
  setObjectName(QStringLiteral("dspwidget"));

  setHidden(!Settings::Instance().IsDSPVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  QSettings& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("dspwidget/geometry")).toByteArray());
  m_splitter->restoreState(
      settings.value(QStringLiteral("dspwidget/splitter")).toByteArray());

  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("dspwidget/floating")).toBool());

  connect(&Settings::Instance(), &Settings::DSPVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsDSPVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &DSPWidget::UpdateAndShowPC);
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &DSPWidget::UpdateAndShowPC);
}

DSPWidget::~DSPWidget()
{
  QSettings& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("dspwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("dspwidget/splitter"), m_splitter->saveState());
}

void DSPWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetDSPVisible(false);
}

void DSPWidget::showEvent(QShowEvent* event)
{
  Update();
}

constexpr int IMEM_COLUMN_BREAK = 0;
constexpr int IMEM_COLUMN_CODE = 1;
constexpr int REGS_COLUMN_NAME = 0;
constexpr int REGS_COLUMN_VALUE = 1;

void DSPWidget::CreateWidgets()
{
  const QFont font = Settings::Instance().GetDebugFont();
  const QFontMetrics fm(font);

  m_imem_table = new QTableWidget(0, 2);
  m_imem_table->setContentsMargins(0, 0, 0, 0);
  m_imem_table->setFont(font);
  m_imem_table->setShowGrid(false);
  m_imem_table->verticalHeader()->hide();
  m_imem_table->horizontalHeader()->hide();
  m_imem_table->horizontalHeader()->setStretchLastSection(true);
  m_imem_table->horizontalHeader()->setSectionResizeMode(IMEM_COLUMN_BREAK, QHeaderView::Fixed);
  m_imem_table->horizontalHeader()->setSectionResizeMode(IMEM_COLUMN_CODE, QHeaderView::ResizeToContents);
  m_imem_table->setColumnWidth(IMEM_COLUMN_BREAK, 20);
  m_imem_table->setSelectionBehavior(QTableWidget::SelectRows);
  connect(m_imem_table, &QTableWidget::cellClicked, this, &DSPWidget::OnIMemClick);
  connect(m_imem_table, &QTableWidget::cellChanged, this, &DSPWidget::OnIMemChange);

  // TODO: split rows into individual cells
  m_dmem_table = new QTableWidget(0, 2);
  m_dmem_table->setShowGrid(false);
  m_dmem_table->verticalHeader()->hide();
  m_dmem_table->horizontalHeader()->hide();
  m_dmem_table->horizontalHeader()->setStretchLastSection(true);
  m_dmem_table->setFont(font);

  m_regs_table = new QTableWidget(32, 2);
  m_regs_table->setFont(font);
  m_regs_table->setShowGrid(false);
  m_regs_table->verticalHeader()->hide();
  m_regs_table->horizontalHeader()->hide();
  for (size_t i = 0; i < 32; ++i)
  {
    auto reg_name = new QTableWidgetItem(QString::fromLatin1(DSP::pdregname(i)));
    reg_name->setFlags(reg_name->flags() & ~Qt::ItemIsEditable);
    m_regs_table->setItem(i, REGS_COLUMN_NAME, reg_name);
    m_regs_table->setItem(i, REGS_COLUMN_VALUE, new QTableWidgetItem);
  }
  m_regs_table->horizontalHeader()->setSectionResizeMode(REGS_COLUMN_NAME, QHeaderView::Fixed);
  m_regs_table->horizontalHeader()->setSectionResizeMode(REGS_COLUMN_VALUE, QHeaderView::Fixed);
  m_regs_table->setColumnWidth(REGS_COLUMN_NAME, fm.boundingRect(QStringLiteral("PROD.M2")).width() + 8);
  m_regs_table->setColumnWidth(REGS_COLUMN_VALUE, fm.boundingRect(QStringLiteral("0xAAAA")).width() + 8);
  connect(m_regs_table, &QTableWidget::cellChanged, this, &DSPWidget::OnRegChange);

  m_splitter = new QSplitter(Qt::Horizontal);
  m_splitter->addWidget(m_regs_table);
  m_splitter->addWidget(m_imem_table);
  m_splitter->addWidget(m_dmem_table);
  //m_dmem_table->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  m_regs_table->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

  const auto button_run = new QPushButton(QStringLiteral("Run"));
  const auto button_step = new QPushButton(QStringLiteral("Step"));
  connect(button_run, &QPushButton::clicked, this, &DSPWidget::OnRun);
  connect(button_step, &QPushButton::clicked, this, &DSPWidget::OnStep);

  const auto hbox = new QHBoxLayout;
  hbox->addWidget(button_run);
  hbox->addWidget(button_step);
  hbox->addStretch();

  const auto vbox = new QVBoxLayout;
  vbox->addLayout(hbox);
  vbox->addWidget(m_splitter);

  const auto dummy = new QWidget;
  dummy->setLayout(vbox);
  setWidget(dummy);
}

void DSPWidget::OnRegChange(int row, int column)
{
  if (column == REGS_COLUMN_VALUE)
  {
    Core::CPUThreadGuard guard(m_system);
    if (DSP::DSPCore* dsp = GetDSPCore(guard))
    {
      QTableWidgetItem* item = m_regs_table->item(row, column);
      bool ok;
      u16 value = item->text().toUShort(&ok, 16);
      if (ok)
        dsp->WriteRegister(row, value);
    }
  }
}

void DSPWidget::UpdateAndShowPC()
{
  Update();
  if (m_pc_row != -1)
    m_imem_table->scrollToItem(m_imem_table->item(m_pc_row, 0));
}

DSP::DSPCore* DSPWidget::GetDSPCore(const Core::CPUThreadGuard &guard)
{
  DSPEmulator* emu = guard.GetSystem().GetDSP().GetDSPEmulator();
  if (!emu || !emu->IsLLE())
    return nullptr;
  return &static_cast<DSP::LLE::DSPLLE*>(emu)->GetDSPCore();
}

void DSPWidget::Update()
{
  QSignalBlocker blocker1(m_regs_table);
  QSignalBlocker blocker2(m_imem_table);
  m_imem_table->setRowCount(0);
  m_dmem_table->setRowCount(0);
  Core::CPUThreadGuard guard(m_system);
  DSP::DSPCore* dsp = GetDSPCore(guard);
  if (!dsp)
    return;
  DSP::SDSP& state = dsp->DSPState();
  if (!state.iram)
    return;
  for (int i = 0; i < 32; ++i)
  {
    QTableWidgetItem* item = m_regs_table->item(i, REGS_COLUMN_VALUE);
    u16 value = state.ReadRegister(i);
    const QString new_value = QString::fromStdString(fmt::format("0x{:04X}", value));
    const QString old_value = m_regs_table->item(i, REGS_COLUMN_VALUE)->text();
    item->setText(new_value);
    if (new_value == old_value)
      item->setForeground(Qt::GlobalColor::color0);
    else
      item->setForeground(Qt::GlobalColor::red);
  }

  const QFont font = Settings::Instance().GetDebugFont();
  const QFontMetrics fm(font);
  m_pc_row = -1;
  const int total_imem_size = DSP::DSP_IRAM_SIZE + DSP::DSP_IROM_SIZE;
  for (int line = 0; line < total_imem_size; ++line)
  {
    if (DSP::Symbols::Addr2Line(state.pc) == line)
      m_pc_row = m_imem_table->rowCount();
    std::string asm_str;
    asm_str = DSP::Symbols::GetLineText(line);
    if (asm_str == "----")
      break;
    const auto icon_item = new QTableWidgetItem();
    if (dsp->BreakPoints().IsAddressBreakPoint(DSP::Symbols::Line2Addr(line)))
    {
      icon_item->setData(
          Qt::DecorationRole,
          Resources::GetThemeIcon("debugger_breakpoint").pixmap(QSize(10, 10)));
    }
    const auto asm_item = new QTableWidgetItem(QString::fromStdString(asm_str));
    m_imem_table->insertRow(m_imem_table->rowCount());
    m_imem_table->setRowHeight(m_imem_table->rowCount() - 1, fm.height());
    m_imem_table->setItem(m_imem_table->rowCount() - 1, IMEM_COLUMN_BREAK, icon_item);
    m_imem_table->setItem(m_imem_table->rowCount() - 1, IMEM_COLUMN_CODE, asm_item);
  }

  // make current (next to execute) instruction bold
  if (m_pc_row != -1)
  {
    auto* cur_inst = m_imem_table->item(m_pc_row, IMEM_COLUMN_CODE);
    QFont inst_font = cur_inst->font();
    inst_font.setBold(true);
    cur_inst->setFont(inst_font);
  }

  const auto add_dmem = [&](const u16* dmem, const u16 start, const u16 size)
  {
    for (size_t i = 0; i < size; i += 8)
    {
      const std::string addr_str = fmt::format("{:04X}", start + i);
      std::string data_str;
      for (size_t j = 0; j < 8; ++j)
      {
        data_str += fmt::format("{:04X} ", dmem[i + j]);
      }
      const auto addr_item = new QTableWidgetItem(QString::fromStdString(addr_str));
      const auto data_item = new QTableWidgetItem(QString::fromStdString(data_str));
      m_dmem_table->insertRow(m_dmem_table->rowCount());
      m_dmem_table->setRowHeight(m_dmem_table->rowCount() - 1, fm.height());
      m_dmem_table->setItem(m_dmem_table->rowCount() - 1, 0, addr_item);
      m_dmem_table->setItem(m_dmem_table->rowCount() - 1, 1, data_item);
    }
  };
  add_dmem(state.dram, 0, DSP::DSP_DRAM_SIZE);
  add_dmem(state.coef, DSP::DSP_DRAM_SIZE, DSP::DSP_COEF_SIZE);
}

void DSPWidget::OnRun()
{
  Core::CPUThreadGuard guard(m_system);
  if (DSP::DSPCore* dsp = GetDSPCore(guard))
    dsp->SetState(DSP::State::Running);
}

void DSPWidget::OnStep()
{
  Core::CPUThreadGuard guard(m_system);
  if (DSP::DSPCore* dsp = GetDSPCore(guard))
  {
    dsp->SetState(DSP::State::Stepping);
    dsp->Step();
  }
}

void DSPWidget::OnIMemClick(int row, int column)
{
  if (column == IMEM_COLUMN_BREAK)
  {
    Core::CPUThreadGuard guard(m_system);
    DSP::DSPCore* dsp = GetDSPCore(guard);
    u32 address = DSP::Symbols::Line2Addr(row);
    if (dsp->BreakPoints().IsAddressBreakPoint(row))
      dsp->BreakPoints().DeleteByAddress(address);
    else
      dsp->BreakPoints().Add(address);
    Update();
  }
}

void DSPWidget::OnIMemChange(int row, int column)
{
  if (column == IMEM_COLUMN_CODE)
  {
    Core::CPUThreadGuard guard(m_system);
    DSP::DSPCore* dsp = GetDSPCore(guard);
    if (!dsp)
      return;
    DSP::SDSP& state = dsp->DSPState();
    u32 address = DSP::Symbols::Line2Addr(row);
    QTableWidgetItem* item = m_imem_table->item(row, column);
    DSP::AssemblerSettings settings;
    settings.show_pc = true;
    settings.show_hex = true;
    DSP::DSPAssembler assembler(settings);
    std::vector<u16> code;
    // FIXME: doesn't work since address and hex are still in the same cell
    if (assembler.Assemble(item->text().toStdString(), code))
    {
      // TODO: move this out of Qt code
      Common::UnWriteProtectMemory(state.iram, DSP::DSP_IRAM_BYTE_SIZE, false);
      state.iram[address] = code[0];
      Common::WriteProtectMemory(state.iram, DSP::DSP_IRAM_BYTE_SIZE, false);
      DSP::Symbols::Clear();
      DSP::Symbols::AutoDisassembly(state, 0x0, 0x1000);
      DSP::Symbols::AutoDisassembly(state, 0x8000, 0x9000);
    }
    Update();
  }
}
