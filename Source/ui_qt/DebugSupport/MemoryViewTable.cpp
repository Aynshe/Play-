#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QHeaderView>
#include <QFont>
#include <QVBoxLayout>

#include "string_format.h"
#include "MemoryViewTable.h"
#include "DebugExpressionEvaluator.h"

CMemoryViewTable::CMemoryViewTable(QWidget* parent)
    : QTableView(parent)
{
	m_model = new CQtMemoryViewModel(this);

	setModel(m_model);

	// prepare monospaced font
	QFont fixedFont = QFont("Courier New", 8);
	setFont(fixedFont);

	QFontMetrics metric(fixedFont);
	m_cwidth = metric.maxWidth();

	auto header = horizontalHeader();
	header->setMinimumSectionSize(m_cwidth);
	header->hide();
	ResizeColumns();

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QTableView::customContextMenuRequested, this, &CMemoryViewTable::ShowContextMenu);
	connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &CMemoryViewTable::SelectionChanged);
}

void CMemoryViewTable::Setup(CVirtualMachine* virtualMachine, CMIPS* ctx, bool memoryJumps)
{
	m_virtualMachine = virtualMachine;
	m_context = ctx;
	m_enableMemoryJumps = memoryJumps;
	if(m_enableMemoryJumps)
	{
		assert(m_virtualMachine);
		assert(m_context);
	}
}

void CMemoryViewTable::SetData(CQtMemoryViewModel::getByteProto getByte, int size)
{
	m_model->SetData(getByte, size);
}

void CMemoryViewTable::ShowEvent()
{
	AutoColumn();
}

void CMemoryViewTable::ResizeEvent()
{
	if(!m_model || m_bytesPerLine)
		return;

	AutoColumn();
}

void CMemoryViewTable::HandleMachineStateChange()
{
	m_model->Redraw();
}

int CMemoryViewTable::GetBytesPerLine()
{
	return m_bytesPerLine;
}

void CMemoryViewTable::SetBytesPerLine(int bytesForLine)
{
	m_bytesPerLine = bytesForLine;
	if(bytesForLine)
	{
		m_model->SetColumnCount(bytesForLine);
		ResizeColumns();
		m_model->Redraw();
	}
	else
	{
		AutoColumn();
	}
}

void CMemoryViewTable::ShowContextMenu(const QPoint& pos)
{
	auto rightClickMenu = new QMenu(this);

	{
		auto bytesPerLineMenu = rightClickMenu->addMenu(tr("&Bytes Per Line"));
		auto bytesPerLineActionGroup = new QActionGroup(this);
		bytesPerLineActionGroup->setExclusive(true);

		auto autoAction = bytesPerLineMenu->addAction("auto");
		autoAction->setChecked(m_bytesPerLine == 0);
		autoAction->setCheckable(true);
		bytesPerLineActionGroup->addAction(autoAction);
		connect(autoAction, &QAction::triggered, std::bind(&CMemoryViewTable::SetBytesPerLine, this, 0));

		auto byte16Action = bytesPerLineMenu->addAction("16 bytes");
		byte16Action->setChecked(m_bytesPerLine == 16);
		byte16Action->setCheckable(true);
		bytesPerLineActionGroup->addAction(byte16Action);
		connect(byte16Action, &QAction::triggered, std::bind(&CMemoryViewTable::SetBytesPerLine, this, 16));

		auto byte32Action = bytesPerLineMenu->addAction("32 bytes");
		byte32Action->setChecked(m_bytesPerLine == 32);
		byte32Action->setCheckable(true);
		bytesPerLineActionGroup->addAction(byte32Action);
		connect(byte32Action, &QAction::triggered, std::bind(&CMemoryViewTable::SetBytesPerLine, this, 32));
	}

	{
		auto unitMenu = rightClickMenu->addMenu(tr("&Display Unit"));
		auto unitActionGroup = new QActionGroup(this);
		unitActionGroup->setExclusive(true);

		auto activeUnit = m_model->GetActiveUnit();
		for(uint32 i = 0; i < CQtMemoryViewModel::g_units.size(); i++)
		{
			const auto& unit = CQtMemoryViewModel::g_units[i];
			auto itemAction = unitMenu->addAction(unit.description);
			itemAction->setChecked(i == activeUnit);
			itemAction->setCheckable(true);
			unitActionGroup->addAction(itemAction);
			connect(itemAction, &QAction::triggered, std::bind(&CMemoryViewTable::SetActiveUnit, this, i));
		}
	}

	if(m_enableMemoryJumps)
	{
		rightClickMenu->addSeparator();
		{
			auto itemAction = rightClickMenu->addAction("Goto Address...");
			connect(itemAction, &QAction::triggered, std::bind(&CMemoryViewTable::GotoAddress, this));
		}

		{
			uint32 selection = m_selected;
			if((selection & 0x03) == 0)
			{
				uint32 valueAtSelection = m_context->m_pMemoryMap->GetWord(selection);
				auto followPointerText = string_format("Follow Pointer (0x%08X)", valueAtSelection);
				auto itemAction = rightClickMenu->addAction(followPointerText.c_str());
				connect(itemAction, &QAction::triggered, std::bind(&CMemoryViewTable::FollowPointer, this));
			}
		}
	}

	rightClickMenu->popup(viewport()->mapToGlobal(pos));
}

void CMemoryViewTable::ResizeColumns()
{
	auto header = horizontalHeader();
	header->setSectionResizeMode(QHeaderView::Fixed);

	auto units = m_model->columnCount() - 1;
	auto bytesPerUnit = m_model->GetBytesPerUnit();

	auto valueCell = m_cwidth * (m_model->CharsPerUnit() + 2);
	int asciiCell = (m_cwidth * bytesPerUnit * units) + (m_cwidth * 2);
	for(auto i = 0; i < units; ++i)
	{
		header->resizeSection(i, valueCell);
	}
	header->resizeSection(units, asciiCell);

	// collapse unused column
	for(auto i = units + 1; i < m_maxUnits; ++i)
	{
		header->resizeSection(i, 0);
	}
	m_maxUnits = units + 1;
}

void CMemoryViewTable::AutoColumn()
{
	if(m_bytesPerLine)
		return;

	QFont font = this->font();
	QFontMetrics metric(font);
	int columnHeaderWidth = metric.horizontalAdvance(" 0x00000000");

	int tableWidth = size().width() - columnHeaderWidth - style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	auto bytesPerUnit = m_model->GetBytesPerUnit();

	int valueCell = m_cwidth * (m_model->CharsPerUnit() + 2);
	int asciiCell = m_cwidth * bytesPerUnit;

	int i = 0x2;
	while(true)
	{
		int valueCellsWidth = i * valueCell;
		int asciiCellsWidth = (i * asciiCell) + (m_cwidth * 2);
		int totalWidth = valueCellsWidth + asciiCellsWidth;
		if(totalWidth > tableWidth)
		{
			--i;
			break;
		}
		++i;
	}
	m_model->SetColumnCount(i * bytesPerUnit);
	ResizeColumns();
	m_model->Redraw();
}

void CMemoryViewTable::GotoAddress()
{
	if(m_virtualMachine->GetStatus() == CVirtualMachine::RUNNING)
	{
		QApplication::beep();
		return;
	}

	std::string sValue;
	{
		bool ok;
		QString res = QInputDialog::getText(this, tr("Goto Address"),
		                                    tr("Enter new address:"), QLineEdit::Normal,
		                                    tr("00000000"), &ok);
		if(!ok || res.isEmpty())
			return;

		sValue = res.toStdString();
	}

	try
	{
		uint32 nAddress = CDebugExpressionEvaluator::Evaluate(sValue.c_str(), m_context);
		SetSelectionStart(nAddress);
	}
	catch(const std::exception& exception)
	{
		std::string message = std::string("Error evaluating expression: ") + exception.what();
		QMessageBox::critical(this, tr("Error"), tr(message.c_str()), QMessageBox::Ok, QMessageBox::Ok);
	}
}

void CMemoryViewTable::FollowPointer()
{
	if(m_virtualMachine->GetStatus() == CVirtualMachine::RUNNING)
	{
		QApplication::beep();
		return;
	}

	uint32 valueAtSelection = m_context->m_pMemoryMap->GetWord(m_selected);
	SetSelectionStart(valueAtSelection);
}

void CMemoryViewTable::SetActiveUnit(int index)
{
	m_model->SetActiveUnit(index);
	AutoColumn();

	ResizeColumns();
	m_model->Redraw();
}

void CMemoryViewTable::SetSelectionStart(uint32 address)
{
	auto column = address % m_model->BytesForCurrentLine();
	auto row = (address - column) / m_model->BytesForCurrentLine();

	auto index = m_model->index(row, column);
	selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
	scrollTo(index, QAbstractItemView::PositionAtCenter);
}

void CMemoryViewTable::SelectionChanged()
{
	auto indexes = selectionModel()->selectedIndexes();
	if(!indexes.empty())
	{
		auto index = indexes.first();
		int address = index.row() * (m_model->BytesForCurrentLine());
		if(m_model->columnCount() -1 != index.column())
		{
			address += index.column() * m_model->GetBytesPerUnit();
		}
		m_selected = address;
		OnSelectionChange(m_selected);
	}
}
