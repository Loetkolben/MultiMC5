/* Copyright 2013 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VERSION_H
#define VERSION_H

#include <QObject>

class Version : public QObject
{
	Q_OBJECT
public:
	explicit Version(int major = 0, int minor = 0, int revision = 0, 
					 int build = 0, QObject *parent = 0);
	
	Version(const Version& ver);
	
	QString toString() const;
	
	int major;
	int minor;
	int revision;
	int build;
	
	static Version current;
};

#endif // VERSION_H