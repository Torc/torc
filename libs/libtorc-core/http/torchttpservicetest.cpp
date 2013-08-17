/* Class TorcHTTPServiceTest
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torcadminthread.h"
#include "torchttpservicetest.h"

/*! \class TorcHTTPServiceTest
 *  \brief Implements an HTTP service for testing purposes.
 *
 * This class is purely for testing the HTTP interface.
 *
 * \todo Only build in debug mode.
*/
TorcHTTPServiceTest::TorcHTTPServiceTest()
  : QObject(),
    TorcHTTPService(this, "/test", tr("Test"), TorcHTTPServiceTest::staticMetaObject)
{
}

///\brief Returns a void.
void TorcHTTPServiceTest::GetVoid(void)
{
}

///\brief Returns the given Integer.
int TorcHTTPServiceTest::EchoInt(int Value)
{
    return Value;
}

///\brief Returns the given Boolean value.
bool TorcHTTPServiceTest::EchoBool(bool Value)
{
    return Value;
}

///\brief Returns the given float.
float TorcHTTPServiceTest::EchoFloat(float Value)
{
    return Value;
}

///\brief Returns the given large unsigned integer.
unsigned long long TorcHTTPServiceTest::EchoUnsignedLongLong(unsigned long long Value)
{
    return Value;
}

///\brief Returns the given strings as a list or array.
QStringList TorcHTTPServiceTest::EchoStringList(const QString &Value1, const QString &Value2, const QString &Value3)
{
    QStringList result;
    result << Value1 << Value2 << Value3;
    return result;
}

///\brief Returns a complex, nested data type.
QVariantMap TorcHTTPServiceTest::GetVariantMap(void)
{
    QVariantMap level1;
    QVariantMap level2;
    QStringList level3;

    level3 << "A" << "B" << "C";
    level2.insert("LevelThree", level3);

    level1.insert("One   float", 1.1234);
    level1.insert("Two   text",  "Two");
    level1.insert("Three int",   3);
    level1.insert("LevelTwo", level2);

    return level1;
}

///\brief Returns the current time.
QDateTime TorcHTTPServiceTest::GetTimeNow(void)
{
    return QDateTime::currentDateTime();
}

///\brief Returns the current time in Utc.
QDateTime TorcHTTPServiceTest::GetTimeNowUtc(void)
{
    return QDateTime::currentDateTimeUtc();
}

///\brief This function should be rejected at startup.
QVariantHash TorcHTTPServiceTest::GetUnsupportedHash(void)
{
    QVariantHash result;
    result.insert("foo", "bar");
    return result;
}

///\brief This function should be rejected at startup.
void* TorcHTTPServiceTest::GetUnsupportedPointer(void)
{
    return NULL;
}

///\brief This function should be rejected at startup
void TorcHTTPServiceTest::GetUnsupportedParameter(QStringList Unsupported)
{
    (void)Unsupported;
}

/*! \class TorcServiceTestObject
 *  \brief Creates a TorcHTTPServiceTest singleton.
*/
static class TorcServiceTestObject : public TorcAdminObject
{
  public:
    TorcServiceTestObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY),
        m_test(NULL)
    {
    }

    void Create(void)
    {
        m_test = new TorcHTTPServiceTest();
    }

    void Destroy(void)
    {
        delete m_test;
    }

  private:
    TorcHTTPServiceTest *m_test;

} TorcServiceTestObject;


