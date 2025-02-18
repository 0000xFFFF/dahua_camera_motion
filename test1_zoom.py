#!/usr/bin/env python3
import cv2
import numpy as np

def motion_detection(ip, user, password):
    """Detect motion in the RTSP stream using OpenCV and zoom in on motion area."""
    rtsp_url = f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel=0&subtype=0"
    cap = cv2.VideoCapture(rtsp_url)

    if not cap.isOpened():
        print("Error: Could not open RTSP stream.")
        return

    fgbg = cv2.createBackgroundSubtractorMOG2(history=500, varThreshold=16, detectShadows=True)
    zoom_active = False
    zoom_rect = (0, 0, 235, 192)

    while True:
        ret, frame = cap.read()
        
        if not ret:
            print("Error: Failed to grab frame.")
            break
        
        # Crop the frame
        crop_width = 704
        crop_height = 384
        crop_x = 0
        crop_y = 0
        frame = frame[crop_y:crop_y + crop_height, crop_x:crop_x + crop_width]
        
        fgmask = fgbg.apply(frame)
        _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        max_area = 0
        best_rect = None

        for contour in contours:
            if cv2.contourArea(contour) > 20:
                x, y, w, h = cv2.boundingRect(contour)
                if cv2.contourArea(contour) > max_area:
                    max_area = cv2.contourArea(contour)
                    best_rect = (x, y, w, h)
        
        if best_rect:
            x, y, w, h = best_rect
            zoom_x = max(0, x + w//2 - 235//2)
            zoom_y = max(0, y + h//2 - 192//2)
            zoom_rect = (zoom_x, zoom_y, 235, 192)
            zoom_active = True
        else:
            zoom_active = False
        
        if zoom_active:
            zx, zy, zw, zh = zoom_rect
            zoomed_frame = frame[zy:zy+zh, zx:zx+zw]
            zoomed_frame = cv2.resize(zoomed_frame, (1800, 1000))
        else:
            zoomed_frame = cv2.resize(frame, (1800, 1000))
        
        cv2.imshow('Motion Detection', zoomed_frame)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <IP> <USER> <PASS>")
        sys.exit(1)
    
    motion_detection(sys.argv[1], sys.argv[2], sys.argv[3])

