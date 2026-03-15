import streamlit as st
import requests
import time
import pandas as pd

ESP32_URL = "http://192.168.1.6/status"

st.set_page_config(layout="wide")
st.title("GNSS Live Data Monitor")

# Memory storage
if "history" not in st.session_state:
    st.session_state.history = []

status_box = st.empty()
raw_box = st.empty()
change_box = st.empty()
graph_box = st.empty()

previous_data = None

while True:
    try:
        r = requests.get(ESP32_URL, timeout=2)
        data = r.json()

        status_box.success("ESP32 Connected")

        raw_box.json(data)

        # Detect change
        if previous_data is not None:
            if data != previous_data:
                change_box.success("Live Data Updating")
            else:
                change_box.warning("Data NOT Changing")
        previous_data = data

        # Store history
        st.session_state.history.append({
            "sat": float(data.get("sat",0)),
            "hdop": float(data.get("hdop",0)),
            "drift": float(data.get("drift",0))
        })

        df = pd.DataFrame(st.session_state.history)

        graph_box.line_chart(df)

    except Exception as e:
        status_box.error("ESP32 Not Connected")
        raw_box.write(e)

    time.sleep(1)