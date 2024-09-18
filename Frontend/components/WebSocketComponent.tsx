"use client";
import {
  faHeart,
  faTemperatureThreeQuarters,
  faPersonWalking,
  faSun,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import React, { useEffect, useRef, useState } from "react";

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

  const RATE_SIZE = 4; // Number of heart rates to average
  const MINIMUM_DELTA = 300; // Minimum time between two beats (in milliseconds)
  const MAXIMUM_DELTA = 1500; // Maximum time between two beats (in milliseconds)

  // UseRef for mutable variables that don't cause re-renders
  const rates = useRef<number[]>(new Array(RATE_SIZE).fill(0));
  const rateSpot = useRef<number>(0);
  const lastBeat = useRef<number>(0); // Keep track of the time of the last valid beat

  const handDetected = useRef<boolean>(false); // Track hand detection status

  useEffect(() => {
    const websocket = new WebSocket("ws://192.168.7.97/ws");

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
          setTemp(data.dt.T);
          setUV(data.dt.U);
        }

        if (data.t === "s") {
          setStepCount(data.dt.s);
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

  return (
    <div className="main-div">
      <h1>Result Monitor Dashboard</h1>
      <div className="data">
        {/* <p>Red: {red}</p>
        <p>IR: {ir}</p>
        <p>Green: {green}</p>
        <p>Temperature: {temp}°C</p>
        <p>UV Index: {uv}</p>
        <p>Step count: {stepCount}</p>
        <p>Beats per minute: {beatsPerMinute}</p>
        <p>Average BPM: {beatAvg}</p> */}
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
          <p>TEMP: {temp.toFixed(2)}°C </p>
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
      <h2>Average BPM: {beatAvg.toFixed(2)}</h2>
    </div>
  );
};

export default WebSocketComponent;
