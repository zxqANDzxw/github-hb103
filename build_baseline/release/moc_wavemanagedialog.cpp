/****************************************************************************
** Meta object code from reading C++ file 'wavemanagedialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.9)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../wavemanagedialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'wavemanagedialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.9. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_WaveManageDialog_t {
    QByteArrayData data[11];
    char stringdata0[143];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_WaveManageDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_WaveManageDialog_t qt_meta_stringdata_WaveManageDialog = {
    {
QT_MOC_LITERAL(0, 0, 16), // "WaveManageDialog"
QT_MOC_LITERAL(1, 17, 21), // "on_pushButton_clicked"
QT_MOC_LITERAL(2, 39, 0), // ""
QT_MOC_LITERAL(3, 40, 18), // "onWaveListReceived"
QT_MOC_LITERAL(4, 59, 19), // "QList<WaveFileInfo>"
QT_MOC_LITERAL(5, 79, 8), // "waveList"
QT_MOC_LITERAL(6, 88, 14), // "onDownloadFile"
QT_MOC_LITERAL(7, 103, 8), // "fileName"
QT_MOC_LITERAL(8, 112, 3), // "row"
QT_MOC_LITERAL(9, 116, 13), // "onAnalyzeFile"
QT_MOC_LITERAL(10, 130, 12) // "onDeleteFile"

    },
    "WaveManageDialog\0on_pushButton_clicked\0"
    "\0onWaveListReceived\0QList<WaveFileInfo>\0"
    "waveList\0onDownloadFile\0fileName\0row\0"
    "onAnalyzeFile\0onDeleteFile"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_WaveManageDialog[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   39,    2, 0x08 /* Private */,
       3,    1,   40,    2, 0x08 /* Private */,
       6,    2,   43,    2, 0x08 /* Private */,
       9,    2,   48,    2, 0x08 /* Private */,
      10,    2,   53,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    7,    8,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    7,    8,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    7,    8,

       0        // eod
};

void WaveManageDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        WaveManageDialog *_t = static_cast<WaveManageDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->on_pushButton_clicked(); break;
        case 1: _t->onWaveListReceived((*reinterpret_cast< const QList<WaveFileInfo>(*)>(_a[1]))); break;
        case 2: _t->onDownloadFile((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 3: _t->onAnalyzeFile((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 4: _t->onDeleteFile((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<WaveFileInfo> >(); break;
            }
            break;
        }
    }
}

const QMetaObject WaveManageDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_WaveManageDialog.data,
      qt_meta_data_WaveManageDialog,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *WaveManageDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WaveManageDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_WaveManageDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int WaveManageDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
