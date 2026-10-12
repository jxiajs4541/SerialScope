#include "DbConfig.h"

// ODBC connection string for the MySQL log database.
// NOTE: replace YOUR_PASSWORD (and SERVER / UID if needed) with your local values.
QString dbDatabaseName() {
	return "DRIVER={MySQL ODBC 26.7 Unicode Driver};"
		"SERVER=localhost;PORT=3306;"
		"DATABASE=serial_db;UID=root;PWD=YOUR_PASSWORD;"
		"CHARSET=utf8mb4;";
}