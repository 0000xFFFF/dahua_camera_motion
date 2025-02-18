#!/usr/bin/env python3
import cv2
import numpy as np
import sys

def start_capture(ip, user, password, channel):
    """Start OpenCV VideoCapture for a given RTSP channel."""
    if channel == 0:
        width, height = 704, 576
    else:
        width, height = 1920, 1080
    
    rtsp_url = f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel={channel}&subtype=0"
    cap = cv2.VideoCapture(rtsp_url)
    return cap, width, height

def motion_detection(ip, user, password):
    """Detect motion in multiple RTSP streams and display accordingly."""
    caps = {}
    fgbg = {ch: cv2.createBackgroundSubtractorMOG2() for ch in range(7)}

    # Start capturing from all channels
    for ch in range(7):
        caps[ch], width, height = start_capture(ip, user, password, ch)
    
    while True:
        display_frame = None
        
        for ch in range(7):
            ret, frame = caps[ch].read()
            if not ret:
                continue  # Skip if frame is not available
            
            fgmask = fgbg[ch].apply(frame)
            _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            for contour in contours:
                if cv2.contourArea(contour) > 10000:
                    (x, y, w, h) = cv2.boundingRect(contour)
                    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                    display_frame = frame
                    break
            
            if display_frame is not None:
                break
        
        if display_frame is None:
            ret, display_frame = caps[0].read()
            if not ret:
                continue  # Skip if no fallback frame is available
        
        resized_frame = cv2.resize(display_frame, (1800, 1000))
        cv2.imshow('Motion Detection', resized_frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    for cap in caps.values():
        cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <IP> <USER> <PASS>")
        sys.exit(1)
    
    ip, user, password = sys.argv[1:]
    motion_detection(ip, user, password)

