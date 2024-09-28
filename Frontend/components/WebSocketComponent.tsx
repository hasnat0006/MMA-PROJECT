"use client";
import {
  faHeart,
  faPersonWalking,
  faSun,
  faTemperatureThreeQuarters,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import React, { useEffect, useRef, useState } from "react";
import { ToastContainer, toast } from "react-toastify";
import "react-toastify/dist/ReactToastify.css";

import "./Page.css";

const WebSocketComponent: React.FC = () => {
  const [ws, setWs] = useState<WebSocket | null>(null);
  const [red, setRed] = useState<number>(0);
  const [ir, setIR] = useState<number>(0);
  const [green, setGreen] = useState<number>(0);
  const [temp, setTemp] = useState<number>(0);
  const [stepCount, setStepCount] = useState<number>(0);
  const [beatsPerMinute, setBeatsPerMinute] = useState<number>(0); // Set to 0 when no hand is detected
  const [beatAvg, setBeatAvg] = useState<number>(0); // Set to 0 when no hand is detected
  const [uv, setUV] = useState<number>(0);
  const [calories, setCalories] = useState<number>(0);

  const RATE_SIZE = 4;
  const MINIMUM_DELTA = 300;
  const MAXIMUM_DELTA = 1500;

  const rates = useRef<number[]>(new Array(RATE_SIZE).fill(0));
  const rateSpot = useRef<number>(0);
  const lastBeat = useRef<number>(0);

  const handDetected = useRef<boolean>(false);

  useEffect(() => {
    const websocket = new WebSocket("ws://192.168.182.97/ws");

    websocket.onopen = () => {
      console.log("Connected to WebSocket server");
    };

    websocket.onmessage = (event: MessageEvent) => {
      try {
        const data = JSON.parse(event.data);
        console.log(data);

        if (data.t === "rig") {
          setRed(data.dt.r);
          setIR(data.dt.ir);
          setGreen(data.dt.g);

          if (data.dt.ir > 50000) {
            // Hand is placed over the sensor, process heart rate
            if (!handDetected.current) {
              // Reset lastBeat to enable re-detection
              lastBeat.current = 0;
              handDetected.current = true;
            }
            processHeartRate(data.dt.ir);
          } else {
            // Hand removed from the sensor, reset heart rate
            if (handDetected.current) {
              resetHeartRate();
              handDetected.current = false;
            }
          }
        }

        if (data.t === "tmp") {
          const tempF = (data.dt.T * 9) / 5 + 32;
          setTemp(tempF);
          setUV(data.dt.U);
        }

        if (data.t === "s") {
          setStepCount(data.dt.s);
          // calculateCalories();
        }
      } catch (error) {
        console.error("Error parsing WebSocket message:", event.data, error);
      }
    };

    websocket.onerror = (event: Event) => {
      console.log("WebSocket error", event);
    };

    websocket.onclose = () => {
      console.log("WebSocket connection closed");
    };

    setWs(websocket);

    return () => {
      websocket.close();
    };
  }, []);

  useEffect(() => {
    calculateCalories();
    if (beatsPerMinute > 120) {
      notify();
    } else {
      toast.dismiss();
    }
  }, [stepCount, beatsPerMinute]);

  const processHeartRate = (irValue: number) => {
    const currentTime = new Date().getTime();

    // Initialize lastBeat on first valid beat detection
    if (lastBeat.current === 0) {
      lastBeat.current = currentTime;
      return; // Return since we don't have a valid delta yet
    }

    const delta = currentTime - lastBeat.current;
    console.log(delta, MINIMUM_DELTA, MAXIMUM_DELTA); // Log delta and the limits for debugging

    // Only process if delta is within valid range
    if (delta > MINIMUM_DELTA && delta < MAXIMUM_DELTA) {
      lastBeat.current = currentTime;

      const bpm = 60 / (delta / 1000); // Calculate BPM
      if (bpm < 180 && bpm > 40) {
        rates.current[rateSpot.current] = bpm;
        rateSpot.current = (rateSpot.current + 1) % rates.current.length;

        const avgBPM =
          rates.current.reduce((a, b) => a + b, 0) / rates.current.length;
        setBeatsPerMinute(bpm); // Update only when a valid beat is found
        setBeatAvg(avgBPM); // Update only when a valid beat is found
      }
    }
  };

  const resetHeartRate = () => {
    // Set heart rate and average BPM to 0 when the hand is removed
    setBeatsPerMinute(0);
    setBeatAvg(0);
    rates.current = new Array(RATE_SIZE).fill(0); // Reset the rates array
    rateSpot.current = 0; // Reset the rate index
    lastBeat.current = 0; // Reset lastBeat to be ready for next hand placement
  };

  const calculateCalories = () => {
    const weight = 70; // Weight in kg
    const Stride_Length = 0.73; // Stride length in meters
    const WtPound = weight * 2.20462;

    const Calories_Burned =
      stepCount * (Stride_Length * 0.000473) * (WtPound / 100);
    console.log(Calories_Burned);

    return setCalories(Calories_Burned);
  };

  const notify = () => {
    toast.error("Heart rate is too high", {
      position: "top-right",
      autoClose: 5000,
      hideProgressBar: false,
      closeOnClick: true,
      pauseOnHover: true,
      draggable: true,
      progress: undefined,
    });
  };

  return (
    <div className="main-div">
      <ToastContainer />
      <h1>Health Monitor Dashboard</h1>
      <div className="data">
        <div className="each-element">
          <FontAwesomeIcon icon={faHeart} fade className="icon" />
          <p>BPM: {beatsPerMinute.toFixed(2)}</p>
        </div>
        <div className="each-element">
          <FontAwesomeIcon
            icon={faTemperatureThreeQuarters}
            fade
            className="icon"
          />
          <p>TEMP: {temp.toFixed(2)}°F </p>
        </div>
        <div className="each-element">
          <FontAwesomeIcon icon={faPersonWalking} fade className="icon" />
          <p>Step count: {stepCount}</p>
        </div>
        <div className="each-element">
          <FontAwesomeIcon icon={faSun} fade className="icon" />
          <p>UV Index: {uv.toFixed(2)}</p>
        </div>
      </div>
      <div>
        <h2>Average BPM: {beatAvg.toFixed(2)}</h2>
        <h2>Burned Caloried: {calories.toFixed(3)}cal</h2>
      </div>
    </div>
  );
};

export default WebSocketComponent;
