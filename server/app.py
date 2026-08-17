from flask import Flask, request, render_template
import sqlite3
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

app = Flask(__name__)
LOCAL_TIMEZONE = ZoneInfo("America/New_York")

@app.route("/telemetry", methods=["POST"])
def receive_telemetry(): 
    data = request.get_json()

    connection = sqlite3.connect("smart_desk.db")

    connection.execute(
        """
        INSERT INTO telemetry (
            temperature_c,
            humidity_pct,
            session_active,
            session_elapsed_s
        )
        VALUES (?, ?, ?, ?)
        """,
        (
            data["temperature_c"],
            data["humidity_pct"],
            data["session_active"],
            data["session_elapsed_s"]
        )
    )

    connection.commit()
    connection.close()

    return {"status": "ok"}, 200

def init_db():
    connection = sqlite3.connect("smart_desk.db")

    connection.execute("""
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            temperature_c REAL,
            humidity_pct REAL,
            session_active INTEGER,
            session_elapsed_s INTEGER
        )
    """)

    connection.execute("""
    CREATE TABLE IF NOT EXISTS sessions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        started_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        ended_at DATETIME,
        duration_s INTEGER
    )
    """)

    connection.commit()
    connection.close()

def format_duration(total_seconds):
    total_seconds = int(total_seconds)

    hours = total_seconds // 3600
    minutes = (total_seconds % 3600) // 60
    seconds = total_seconds % 60

    if hours > 0:
        return f"{hours}h {minutes}m"

    if minutes > 0:
        return f"{minutes}m"

    return f"{seconds}s"

@app.route("/")
def dashboard():
    connection = sqlite3.connect("smart_desk.db")
    connection.row_factory = sqlite3.Row

    # Latest reading for the current-status section
    latest = connection.execute("""
        SELECT *
        FROM telemetry
        ORDER BY id DESC
        LIMIT 1
    """).fetchone()

    # Find the beginning and end of today in local time
    now_local = datetime.now(LOCAL_TIMEZONE)

    start_local = now_local.replace(
        hour=0,
        minute=0,
        second=0,
        microsecond=0
    )

    end_local = start_local + timedelta(days=1)

    # Database timestamps are UTC, so convert our local-day boundaries to UTC
    start_utc = start_local.astimezone(timezone.utc).strftime(
        "%Y-%m-%d %H:%M:%S"
    )

    end_utc = end_local.astimezone(timezone.utc).strftime(
        "%Y-%m-%d %H:%M:%S"
    )

    # Get every raw reading from today
    history = connection.execute("""
        SELECT *
        FROM telemetry
        WHERE timestamp >= ?
          AND timestamp < ?
        ORDER BY timestamp ASC
    """, (start_utc, end_utc)).fetchall()

    session_rows = connection.execute("""
        SELECT *
        FROM sessions
        WHERE started_at < ?
        AND (ended_at IS NULL OR ended_at >= ?)
        ORDER BY started_at ASC
        """, (end_utc, start_utc)).fetchall()

    connection.close()

    day_start_utc = start_local.astimezone(timezone.utc)
    day_end_utc = end_local.astimezone(timezone.utc)
    now_utc = datetime.now(timezone.utc)

    total_focus_seconds = 0
    longest_session_seconds = 0
    session_count_today = 0

    for session in session_rows:
        session_start = datetime.strptime(
            session["started_at"],
            "%Y-%m-%d %H:%M:%S"
        ).replace(tzinfo=timezone.utc)

        if session["ended_at"] is None:
            session_end = now_utc
        else:
            session_end = datetime.strptime(
                session["ended_at"],
                "%Y-%m-%d %H:%M:%S"
            ).replace(tzinfo=timezone.utc)

        overlap_start = max(session_start, day_start_utc)
        overlap_end = min(session_end, day_end_utc)

        if overlap_end > overlap_start:
            duration_today = int(
                (overlap_end - overlap_start).total_seconds()
            )

            total_focus_seconds += duration_today
            session_count_today += 1

            longest_session_seconds = max(
                longest_session_seconds,
                duration_today
            )
    focus_time_today = format_duration(total_focus_seconds)

    longest_session_today = format_duration(
        longest_session_seconds
    )
    # Group raw measurements into 5-minute buckets
    buckets = {}

    for row in history:
        timestamp_utc = datetime.strptime(
            row["timestamp"],
            "%Y-%m-%d %H:%M:%S"
        ).replace(tzinfo=timezone.utc)

        timestamp_local = timestamp_utc.astimezone(LOCAL_TIMEZONE)

        bucket_minute = (timestamp_local.minute // 5) * 5

        bucket_time = timestamp_local.replace(
            minute=bucket_minute,
            second=0,
            microsecond=0
        )

        if bucket_time not in buckets:
            buckets[bucket_time] = {
                "temperatures": [],
                "humidities": []
            }

        buckets[bucket_time]["temperatures"].append(
            row["temperature_c"]
        )

        buckets[bucket_time]["humidities"].append(
            row["humidity_pct"]
        )

    timestamps = []
    temperatures = []
    humidities = []

    for bucket_time, values in buckets.items():
        timestamps.append(
            bucket_time.strftime("%I:%M %p")
        )

        temperatures.append(
            round(
                sum(values["temperatures"])
                / len(values["temperatures"]),
                1
            )
        )

        humidities.append(
            round(
                sum(values["humidities"])
                / len(values["humidities"]),
                1
            )
        )

    return render_template(
        "dashboard.html",
        latest=latest,
        timestamps=timestamps,
        temperatures=temperatures,
        humidities=humidities,
        focus_time_today=focus_time_today,
        session_count_today=session_count_today,
        longest_session_today=longest_session_today
    )

@app.route("/session", methods=["POST"])
def session_event():
    data = request.get_json()

    event = data.get("event")

    connection = sqlite3.connect("smart_desk.db")

    if event == "start":
        open_session = connection.execute("""
            SELECT id
            FROM sessions
            WHERE ended_at IS NULL
            LIMIT 1
        """).fetchone()

        if open_session is None:
            connection.execute("""
                INSERT INTO sessions DEFAULT VALUES
            """)
            connection.commit()

        connection.close()

        print("Session started")
        return {"status": "ok"}, 200

    elif event == "stop":
        duration_s = data.get("duration_s")

        open_session = connection.execute("""
            SELECT id
            FROM sessions
            WHERE ended_at IS NULL
            ORDER BY id DESC
            LIMIT 1
        """).fetchone()

        if open_session is None:
            connection.close()
            return {
                "status": "error",
                "message": "No open session found"
            }, 409

        connection.execute("""
            UPDATE sessions
            SET ended_at = CURRENT_TIMESTAMP,
                duration_s = ?
            WHERE id = ?
        """, (
            duration_s,
            open_session[0]
        ))

        connection.commit()
        connection.close()

        print("Session stopped")
        return {"status": "ok"}, 200

    else:
        connection.close()

        return {
            "status": "error",
            "message": "Unknown session event"
        }, 400

if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=8000)