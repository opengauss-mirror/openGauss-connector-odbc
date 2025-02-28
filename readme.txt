
/********************************************************************

  PSQLODBC.DLL - A library to talk to the PostgreSQL DBMS using ODBC.


  Copyright (C) 1998          Insight Distribution Systems
  Copyright (C) 1998 - 2013   The PostgreSQL Global Development Group

  Multibyte support was added by Sankyo Unyu Service, (C) 2001.

  The code contained in this library is based on code written by
  Christian Czezatke and Dan McGuirk, (C) 1996.


  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library (see "license.txt"); if not, write to
  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
  02139, USA.


  How to contact the authors:

  email:   pgsql-odbc@postgresql.org
  website: https://odbc.postgresql.org/


***********************************************************************/

compile psqlodbc by following command:
sh ./build.sh -bd $GAUSSHOME # give the path of GAUSSHOME
the build library is placed under install directory.

Obtain the officially released version of ODBC for openGauss Connectors from the following link:
https://opengauss.org/zh/download/

Check the integrity of the software package by following command:
sha256sum openGauss-ODBC-xxx.tar.gz

Compare the obtained SHA256 value with the SHA256 value of the corresponding version on the official 
website. If they match, it indicates that the software package is intact; otherwise, you will need to redownload it.
