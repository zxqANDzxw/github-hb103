/****************************************************************************
** Meta object code from reading C++ file 'iprotocol.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.9)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../iprotocol.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'iprotocol.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.9. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_IProtocol_t {
    QByteArrayData data[21];
    char stringdata0[227];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_IProtocol_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_IProtocol_t qt_meta_stringdata_IProtocol = {
    {
QT_MOC_LITERAL(0, 0, 9), // "IProtocol"
QT_MOC_LITERAL(1, 10, 11), // "requestSend"
QT_MOC_LITERAL(2, 22, 0), // ""
QT_MOC_LITERAL(3, 23, 5), // "frame"
QT_MOC_LITERAL(4, 29, 16), // "sigOneFrameReady"
QT_MOC_LITERAL(5, 46, 12), // "MsgDirection"
QT_MOC_LITERAL(6, 59, 3), // "dir"
QT_MOC_LITERAL(7, 63, 16), // "waveListReceived"
QT_MOC_LITERAL(8, 80, 19), // "QList<WaveFileInfo>"
QT_MOC_LITERAL(9, 100, 8), // "waveList"
QT_MOC_LITERAL(10, 109, 13), // "waveListError"
QT_MOC_LITERAL(11, 123, 3), // "msg"
QT_MOC_LITERAL(12, 127, 20), // "waveDownloadProgress"
QT_MOC_LITERAL(13, 148, 8), // "recvSize"
QT_MOC_LITERAL(14, 157, 9), // "totalSize"
QT_MOC_LITERAL(15, 167, 20), // "waveDownloadFinished"
QT_MOC_LITERAL(16, 188, 8), // "fileName"
QT_MOC_LITERAL(17, 197, 3), // "hdr"
QT_MOC_LITERAL(18, 201, 3), // "cfg"
QT_MOC_LITERAL(19, 205, 3), // "dat"
QT_MOC_LITERAL(20, 209, 17) // "waveDownloadError"

    },
    "IProtocol\0requestSend\0\0frame\0"
    "sigOneFrameReady\0MsgDirection\0dir\0"
    "waveListReceived\0QList<WaveFileInfo>\0"
    "waveList\0waveListError\0msg\0"
    "waveDownloadProgress\0recvSize\0totalSize\0"
    "waveDownloadFinished\0fileName\0hdr\0cfg\0"
    "dat\0waveDownloadError"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_IProtocol[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   49,    2, 0x06 /* Public */,
       4,    2,   52,    2, 0x06 /* Public */,
       7,    1,   57,    2, 0x06 /* Public */,
      10,    1,   60,    2, 0x06 /* Public */,
      12,    2,   63,    2, 0x06 /* Public */,
      15,    4,   68,    2, 0x06 /* Public */,
      20,    1,   77,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QByteArray,    3,
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 5,    3,    6,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void, QMetaType::UInt, QMetaType::UInt,   13,   14,
    QMetaType::Void, QMetaType::QString, QMetaType::QByteArray, QMetaType::QByteArray, QMetaType::QByteArray,   16,   17,   18,   19,
    QMetaType::Void, QMetaType::QString,   11,

       0        // eod
};

void IProtocol::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        IProtocol *_t = static_cast<IProtocol *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->requestSend((*reinterpret_cast< const QByteArray(*)>(_a[1]))); break;
        case 1: _t->sigOneFrameReady((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< MsgDirection(*)>(_a[2]))); break;
        case 2: _t->waveListReceived((*reinterpret_cast< const QList<WaveFileInfo>(*)>(_a[1]))); break;
        case 3: _t->waveListError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 4: _t->waveDownloadProgress((*reinterpret_cast< quint32(*)>(_a[1])),(*reinterpret_cast< quint32(*)>(_a[2]))); break;
        case 5: _t->waveDownloadFinished((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QByteArray(*)>(_a[2])),(*reinterpret_cast< const QByteArray(*)>(_a[3])),(*reinterpret_cast< const QByteArray(*)>(_a[4]))); break;
        case 6: _t->waveDownloadError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<WaveFileInfo> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            typedef void (IProtocol::*_t)(const QByteArray & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::requestSend)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (IProtocol::*_t)(const QByteArray & , MsgDirection );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::sigOneFrameReady)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (IProtocol::*_t)(const QList<WaveFileInfo> & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::waveListReceived)) {
                *result = 2;
                return;
            }
        }
        {
            typedef void (IProtocol::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::waveListError)) {
                *result = 3;
                return;
            }
        }
        {
            typedef void (IProtocol::*_t)(quint32 , quint32 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::waveDownloadProgress)) {
                *result = 4;
                return;
            }
        }
        {
            typedef void (IProtocol::*_t)(const QString & , const QByteArray & , const QByteArray & , const QByteArray & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::waveDownloadFinished)) {
                *result = 5;
                return;
            }
        }
        {
            typedef void (IProtocol::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&IProtocol::waveDownloadError)) {
                *result = 6;
                return;
            }
        }
    }
}

const QMetaObject IProtocol::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_IProtocol.data,
      qt_meta_data_IProtocol,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *IProtocol::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *IProtocol::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_IProtocol.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int IProtocol::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void IProtocol::requestSend(const QByteArray & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void IProtocol::sigOneFrameReady(const QByteArray & _t1, MsgDirection _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void IProtocol::waveListReceived(const QList<WaveFileInfo> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void IProtocol::waveListError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void IProtocol::waveDownloadProgress(quint32 _t1, quint32 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void IProtocol::waveDownloadFinished(const QString & _t1, const QByteArray & _t2, const QByteArray & _t3, const QByteArray & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)), const_cast<void*>(reinterpret_cast<const void*>(&_t4)) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void IProtocol::waveDownloadError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
