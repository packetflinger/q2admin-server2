/*
Copyright (C) 2009 Andrey Nazarov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "server.h"


static unsigned long last_timestamp;
static unsigned long norm_timestamp;

//static sqlite3 *db;

static unsigned long long rowid;
static int numcols;
static int errors;

static int db_query_callback(void *user, int argc, char **argv, char **names)
{
    if (argc > 0)
        rowid = strtoull(argv[0], NULL, 10);

    numcols = argc;
    return 0;
}

static int db_query_execute(sqlite3_callback cb, const char *fmt, ...)
{
    char *sql, *err;
    va_list argptr;
    int ret;

    va_start(argptr, fmt);
    sql = sqlite3_vmprintf(fmt, argptr);
    va_end(argptr);

    ret = sqlite3_exec(db, sql, cb, NULL, &err);
    if (ret) {
        printf("%s\n", err);
        sqlite3_free(err);
        errors++;
    }

    sqlite3_free(sql);
    return ret;
}

#define db_query(...)   db_query_execute(db_query_callback, __VA_ARGS__)
#define db_execute(...) db_query_execute(NULL, __VA_ARGS__)

/*
static void log_client(gclient_t *c)
{
    fragstat_t *fs;
    itemstat_t *is;
    int i, time;

    errors = 0;
    numcols = 0;
    if (db_query("SELECT rowid FROM players WHERE netname=%Q", c->pers.netname))
        return;

    if (!numcols) {
        if (db_execute("INSERT INTO players VALUES(%Q,%lu,%lu)",
                       c->pers.netname, last_timestamp, last_timestamp))
            return;
        rowid = sqlite3_last_insert_rowid(db);
    }

    time = (level.framenum - c->resp.enter_framenum) / HZ;

    db_execute("UPDATE records SET "
               "time=time+%d,"
               "score=score+%d,"
               "deaths=deaths+%d,"
               "damage_given=damage_given+%d,"
               "damage_recvd=damage_recvd+%d "
               "WHERE player_id=%llu AND date=%lu",
               time, c->resp.score, c->resp.deaths,
               c->resp.damage_given, c->resp.damage_recvd,
               rowid, norm_timestamp);

    if (!sqlite3_changes(db)) {
        db_execute("INSERT INTO records VALUES(%llu,%lu,%d,%d,%d,%d,%d)",
                   rowid, norm_timestamp,
                   time, c->resp.score, c->resp.deaths,
                   c->resp.damage_given, c->resp.damage_recvd);
    }

    for (i = 0, fs = c->resp.frags; i < FRAG_TOTAL; i++, fs++) {
        if (fs->kills || fs->deaths || fs->suicides || fs->atts || fs->hits) {
            db_execute("UPDATE frags SET "
                       "kills=kills+%d,"
                       "deaths=deaths+%d,"
                       "suicides=suicides+%d,"
                       "atts=atts+%d,"
                       "hits=hits+%d "
                       "WHERE player_id=%llu AND date=%lu AND frag=%d",
                       fs->kills, fs->deaths, fs->suicides, fs->atts, fs->hits,
                       rowid, norm_timestamp, i);

            if (!sqlite3_changes(db)) {
                db_execute("INSERT INTO frags VALUES(%llu,%lu,%d,%d,%d,%d,%d,%d)",
                           rowid, norm_timestamp, i,
                           fs->kills, fs->deaths, fs->suicides, fs->atts, fs->hits);
            }
        }
    }

    for (i = 0, is = c->resp.items; i < ITEM_TOTAL; i++, is++) {
        if (is->pickups || is->misses || is->kills) {
            db_execute("UPDATE items SET "
                       "pickups=pickups+%d,"
                       "misses=misses+%d,"
                       "kills=kills+%d "
                       "WHERE player_id=%llu AND date=%lu AND item=%d",
                       is->pickups, is->misses, is->kills,
                       rowid, norm_timestamp, i);

            if (!sqlite3_changes(db)) {
                db_execute("INSERT INTO items VALUES(%llu,%lu,%d,%d,%d,%d)",
                           rowid, norm_timestamp, i,
                           is->pickups, is->misses, is->kills);
            }
        }
    }

    db_execute("UPDATE players SET updated=%lu WHERE rowid=%llu", last_timestamp, rowid);
}

static time_t normalize_timestamp(time_t t)
{
    struct tm   *tm;

    if (!(tm = localtime(&t)))
        return -1;

    tm->tm_sec = tm->tm_min = tm->tm_hour = 0;
    return mktime(tm);
}

void G_LogClient(gclient_t *c)
{
    if (!db)
        return;

    last_timestamp = time(NULL);
    norm_timestamp = normalize_timestamp(last_timestamp);

    if (db_execute("BEGIN TRANSACTION"))
        return;

    log_client(c);

    db_execute(errors ? "ROLLBACK" : "COMMIT");
}

void G_LogClients(void)
{
    gclient_t *c;
    int i;

    if (!db)
        return;

    if (!game.clients)
        return;

    last_timestamp = time(NULL);
    norm_timestamp = normalize_timestamp(last_timestamp);

    if (db_execute("BEGIN TRANSACTION"))
        return;

    for (i = 0, c = game.clients; i < game.maxclients; i++, c++) {
        if (c->pers.connected != CONN_SPAWNED)
            continue;

        log_client(c);

        if (errors) {
            db_execute("ROLLBACK");
            return;
        }
    }

    db_execute("COMMIT");
}
*/
static const char schema_orig[] =
"BEGIN TRANSACTION;\n"

"CREATE TABLE IF NOT EXISTS players(\n"
    "netname TEXT PRIMARY KEY,\n"
    "created INT,\n"
    "updated INT\n"
");\n"

"CREATE TABLE IF NOT EXISTS records(\n"
    "player_id INT,\n"
    "date INT,\n"
    "time INT,\n"
    "score INT,\n"
    "deaths INT,\n"
    "damage_given INT,\n"
    "damage_recvd INT\n"
");\n"

"CREATE INDEX IF NOT EXISTS records_idx ON records(player_id,date);\n"

"CREATE TABLE IF NOT EXISTS frags(\n"
    "player_id INT,\n"
    "date INT,\n"
    "frag INT,\n"
    "kills INT,\n"
    "deaths INT,\n"
    "suicides INT,\n"
    "atts INT,\n"
    "hits INT\n"
");\n"

"CREATE INDEX IF NOT EXISTS frags_idx ON frags(player_id,date,frag);\n"

"CREATE TABLE IF NOT EXISTS items(\n"
    "player_id INT,\n"
    "date INT,\n"
    "item INT,\n"
    "pickups INT,\n"
    "misses INT,\n"
    "kills INT\n"
");\n"

"CREATE INDEX IF NOT EXISTS items_idx ON items(player_id,date,item);\n"

"COMMIT;\n";




static const char schema[] =
"BEGIN TRANSACTION;\n"
"CREATE TABLE IF NOT EXISTS server(\n"
        "server_id INT,\n"
        "owner INT,\n"
        "serverkey TEXT,\n"
        "flags INT,\n"
        "name TEXT,\n"
        "ip TEXT,\n"
        "port INT,\n"
        "enabled INT\n"
");\n"

"CREATE INDEX IF NOT EXISTS server_idx ON server(server_id);\n"
"COMMIT;\n";


void OpenDatabase(void)
{
    char *err = NULL;

    //G_CheckFilenameVariable(g_sql_database);

    if (!config.db_file[0]) {
        strcpy(config.db_file, "server.db");
    }

    if (sqlite3_open(config.db_file, &db)) {
        printf("Couldn't open SQLite database: %s\n", sqlite3_errmsg(db));
        goto fail;
    }

    if (sqlite3_exec(db, schema, NULL, NULL, &err)) {
        printf("Couldn't create SQLite database schema: %s\n", err);
        goto fail;
    }

    printf("Using SQLite database '%s'\n", config.db_file);
    return;

fail:
    if (err)
        sqlite3_free(err);
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

void CloseDatabase(void)
{
    if (db) {
        printf("\nClosing SQLite database\n");
        sqlite3_close(db);
        db = NULL;
    }
}

void G_RunDatabase(void)
{
}
