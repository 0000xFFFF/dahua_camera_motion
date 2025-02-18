#!/usr/bin/env python3
import cv2

def motion_detection(ip, user, password):
    """Detect motion in the RTSP stream using OpenCV."""
    rtsp_url = f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel=0&subtype=0"
    cap = cv2.VideoCapture(rtsp_url)

    if not cap.isOpened():
        print("Error: Could not open RTSP stream.")
        return

    fgbg = cv2.createBackgroundSubtractorMOG2(history=500, varThreshold=16, detectShadows=True)

    while True:
        ret, frame = cap.read()

        # Crop the frame
        crop_width = 704
        crop_height = 384
        crop_x = 0
        crop_y = 0
        frame = frame[crop_y:crop_y + crop_height, crop_x:crop_x + crop_width]

        if not ret:
            print("Error: Failed to grab frame.")
            break

        fgmask = fgbg.apply(frame)
        _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        for contour in contours:
            if cv2.contourArea(contour) > 20:
                x, y, w, h = cv2.boundingRect(contour)
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)


        frame = cv2.resize(frame, (1800, 1000)) # resize
        cv2.imshow('Motion Detection', frame)

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

