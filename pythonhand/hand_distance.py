import cv2
import mediapipe as mp
import numpy as np
from flask import Flask, Response

mp_hands = mp.solutions.hands
hands = mp_hands.Hands(max_num_hands=2, min_detection_confidence=0.7)
mp_draw = mp.solutions.drawing_utils

app = Flask(__name__)
cap = cv2.VideoCapture(0)

# Shared distances
thumb_index_distances = [0.0, 0.0]

def generate_frames():
    global thumb_index_distances
    while True:
        success, frame = cap.read()
        if not success:
            continue

        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(frame_rgb)

        if results.multi_hand_landmarks:
            for handLms in results.multi_hand_landmarks:
                mp_draw.draw_landmarks(frame, handLms, mp_hands.HAND_CONNECTIONS)

                # Calculate thumb-index distance
                thumb = handLms.landmark[mp_hands.HandLandmark.THUMB_TIP]
                index = handLms.landmark[mp_hands.HandLandmark.INDEX_FINGER_TIP]

                h, w, _ = frame.shape
                thumb_px = np.array([thumb.x * w, thumb.y * h])
                index_px = np.array([index.x * w, index.y * h])
                dist = np.linalg.norm(thumb_px - index_px)

                # Save distances (first hand = 0, second hand = 1)
                if results.multi_hand_landmarks.index(handLms) < 2:
                    thumb_index_distances[results.multi_hand_landmarks.index(handLms)] = dist

        # Encode frame as JPEG
        ret, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000)
