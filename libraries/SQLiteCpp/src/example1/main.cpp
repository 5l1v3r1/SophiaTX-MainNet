/**
 * @file  main.cpp
 * @brief Two short examples in a row.
 *
 *  Demonstrate how-to use the SQLite++ wrapper
 *
 * Copyright (c) 2012-2013 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>
#include "stdio.h"

#include "../SQLiteC++/Database.h"
#include "../SQLiteC++/Statement.h"
#include "../SQLiteC++/Column.h"
#include "../SQLiteC++/Transaction.h"


/// Object Oriented Basic example
class Example
{
public:
    // Constructor
    Example(void) :
        mDb("example.db3"),                                     // Open a database file in readonly mode
        mQuery(mDb, "SELECT * FROM test WHERE size > :min_size")// Compile a SQL query, containing one parameter (index 1)
    {
    }
    virtual ~Example(void)
    {
    }

    /// List the rows where the "size" column is greater than the provided aParamValue
    void ListGreaterThan (const int aParamValue)
    {
        std::cout << "ListGreaterThan (" << aParamValue << ")\n";

        // Bind the integer value provided to the first parameter of the SQL query
        mQuery.bind(":min_size", aParamValue); // same as mQuery.bind(1, aParamValue);

        // Loop to execute the query step by step, to get one a row of results at a time
        while (mQuery.executeStep())
        {
            std::cout << "row : (" << mQuery.getColumn(0) << ", " << mQuery.getColumn(1) << ", " << mQuery.getColumn(2) << ")\n";
        }

        // Reset the query to be able to use it again later
        mQuery.reset();
    }

private:
    SQLite::Database    mDb;    ///< Database connection
    SQLite::Statement   mQuery; ///< Database prepared SQL query
};


int main (void)
{
    // Basic example (1/5) :
    try
    {
        // Open a database file in readonly mode
        SQLite::Database    db("example.db3");  // SQLITE_OPEN_READONLY
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

        // Test if the 'test' table exists
        bool bExists = db.tableExists("test");
        std::cout << "SQLite table 'test' exists=" << bExists << "\n";

        // Get a single value result with an easy to use shortcut
        std::string value = db.execAndGet("SELECT value FROM test WHERE id=2");
        std::cout << "execAndGet=" << value.c_str() << std::endl;

        // Compile a SQL query, containing one parameter (index 1)
        SQLite::Statement   query(db, "SELECT * FROM test WHERE size > ?");
        std::cout << "SQLite statement '" << query.getQuery().c_str() << "' compiled (" << query.getColumnCount () << " columns in the result)\n";
        // Bind the integer value 1 to the first parameter of the SQL query
        query.bind(1, 1);

        // Loop to execute the query step by step, to get one a row of results at a time
        while (query.executeStep())
        {
            // Demonstrate how to get some typed column value (and the equivalent explicit call)
            int         id      = query.getColumn(0); // = query.getColumn(0).getInt()
          //const char* pvalue  = query.getColumn(1); // = query.getColumn(1).getText()
            std::string value2  = query.getColumn(1); // = query.getColumn(1).getText()
            int         size    = query.getColumn(2); // = query.getColumn(2).getInt()

            std::cout << "row : (" << id << ", " << value2.c_str() << ", " << size << ")\n";
        }

        // Reset the query to use it again
        query.reset();
        std::cout << "SQLite statement '" << query.getQuery().c_str() << "' reseted (" << query.getColumnCount () << " columns in the result)\n";
        // Bind the string value "6" to the first parameter of the SQL query
        query.bind(1, "6");

        while (query.executeStep())
        {
            // Demonstrate that inserting column value in a std:ostream is natural
            std::cout << "row : (" << query.getColumn(0) << ", " << query.getColumn(1) << ", " << query.getColumn(2) << ")\n";
        }
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        abort(); // unexpected error : abort the example program
    }

    ////////////////////////////////////////////////////////////////////////////
    // Object Oriented Basic example (2/5) :
    try
    {
        // Open the database and compile the query
        Example example;

        // Demonstrate the way to use the same query with different parameter values
        example.ListGreaterThan(8);
        example.ListGreaterThan(6);
        example.ListGreaterThan(2);
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        abort(); // unexpected error : abort the example program
    }

    // The execAndGet wrapper example (3/5) :
    try
    {
        // Open a database file in readonly mode
        SQLite::Database    db("example.db3");  // SQLITE_OPEN_READONLY
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

        // WARNING: Be very careful with this dangerous method: you have to
        // make a COPY OF THE result, else it will be destroy before the next line
        // (when the underlying temporary Statement and Column objects are destroyed)
        std::string value = db.execAndGet("SELECT value FROM test WHERE id=2");
        std::cout << "execAndGet=" << value.c_str() << std::endl;
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        abort(); // unexpected error : abort the example program
    }

    ////////////////////////////////////////////////////////////////////////////
    // Simple batch queries example (4/5) :
    try
    {
        // Open a database file in create/write mode
        SQLite::Database    db("test.db3", SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

        db.exec("DROP TABLE IF EXISTS test");

        db.exec("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");

        // first row
        int nb = db.exec("INSERT INTO test VALUES (NULL, \"test\")");
        std::cout << "INSERT INTO test VALUES (NULL, \"test\")\", returned " << nb << std::endl;

        // second row
        nb = db.exec("INSERT INTO test VALUES (NULL, \"second\")");
        std::cout << "INSERT INTO test VALUES (NULL, \"second\")\", returned " << nb << std::endl;

        // update the second row
        nb = db.exec("UPDATE test SET value=\"second-updated\" WHERE id='2'");
        std::cout << "UPDATE test SET value=\"second-updated\" WHERE id='2', returned " << nb << std::endl;

        // Check the results : expect two row of result
        SQLite::Statement   query(db, "SELECT * FROM test");
        std::cout << "SELECT * FROM test :\n";
        while (query.executeStep())
        {
            std::cout << "row : (" << query.getColumn(0) << ", " << query.getColumn(1) << ")\n";
        }

        db.exec("DROP TABLE test");
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        abort(); // unexpected error : abort the example program
    }
    remove("test.db3");

    ////////////////////////////////////////////////////////////////////////////
    // RAII transaction example (5/5) :
    try
    {
        // Open a database file in create/write mode
        SQLite::Database    db("transaction.db3", SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

        db.exec("DROP TABLE IF EXISTS test");

        // Exemple of a successful transaction :
        try
        {
            // Begin transaction
            SQLite::Transaction transaction(db);

            db.exec("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");

            int nb = db.exec("INSERT INTO test VALUES (NULL, \"test\")");
            std::cout << "INSERT INTO test VALUES (NULL, \"test\")\", returned " << nb << std::endl;

            // Commit transaction
            transaction.commit();
        }
        catch (std::exception& e)
        {
            std::cout << "SQLite exception: " << e.what() << std::endl;
            abort(); // unexpected error : abort the example program
        }

        // Exemple of a rollbacked transaction :
        try
        {
            // Begin transaction
            SQLite::Transaction transaction(db);

            int nb = db.exec("INSERT INTO test VALUES (NULL, \"second\")");
            std::cout << "INSERT INTO test VALUES (NULL, \"second\")\", returned " << nb << std::endl;

            nb = db.exec("INSERT INTO test ObviousError");
            std::cout << "INSERT INTO test \"error\", returned " << nb << std::endl;

            abort(); // unexpected success : abort the example program

            // Commit transaction
            transaction.commit();
        }
        catch (std::exception& e)
        {
            std::cout << "SQLite exception: " << e.what() << std::endl;
            // expected error, see above
        }

        // Check the results (expect only one row of result, as the second one has been rollbacked by the error)
        SQLite::Statement   query(db, "SELECT * FROM test");
        std::cout << "SELECT * FROM test :\n";
        while (query.executeStep())
        {
            std::cout << "row : (" << query.getColumn(0) << ", " << query.getColumn(1) << ")\n";
        }
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        abort(); // unexpected error : abort the example program
    }
    remove("transaction.db3");

    return 0;
}
