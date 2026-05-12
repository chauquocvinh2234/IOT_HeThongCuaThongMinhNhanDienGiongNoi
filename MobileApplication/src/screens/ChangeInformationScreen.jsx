import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  TextInput,
  TouchableOpacity,
  KeyboardAvoidingView,
  Platform,
  ScrollView,
  Alert,
  ActivityIndicator,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { SafeAreaView } from 'react-native-safe-area-context';
import { AudioModule, useAudioRecorder, useAudioPlayer, setAudioModeAsync, RecordingPresets, useAudioRecorderState, useAudioPlayerStatus } from 'expo-audio';
import { useAuth } from '../contexts/AuthContext';

export default function ChangeInformationScreen({ navigation }) {
  const { user, login } = useAuth();
  const [fullName, setFullName] = useState(user?.fullname || '');
  const [isLoading, setIsLoading] = useState(false);

  // Audio Recording states
  const [recordedUri, setRecordedUri] = useState(null);

  const recorder = useAudioRecorder(
    {
      ...RecordingPresets.HIGH_QUALITY,
      sampleRate: 16000,
      numberOfChannels: 1,
      bitRate: 128000,
      android: {
        extension: '.m4a',
        outputFormat: 'mpeg4',
        audioEncoder: 'aac',
      },
      ios: {
        extension: '.wav',
        outputFormat: 'lpcm',
        audioQuality: 127,
        linearPCMBitDepth: 16,
        linearPCMIsBigEndian: false,
        linearPCMIsFloat: false,
      },
    }
  );

  const recorderState = useAudioRecorderState(recorder, 500);
  const player = useAudioPlayer(recordedUri);
  const playerStatus = useAudioPlayerStatus(player);

  const isRecording = recorderState.isRecording;
  const isPlaying = playerStatus.playing;

  const startRecording = async () => {
    try {
      setRecordedUri(null);

      if (player.playing) {
        player.pause();
      }

      const permission = await AudioModule.requestRecordingPermissionsAsync();
      if (permission.granted) {
        await setAudioModeAsync({
          allowsRecording: true,
          playsInSilentMode: true,
        });

        await recorder.prepareToRecordAsync();
        recorder.record();
      } else {
        Alert.alert('Cấp quyền', 'Vui lòng cấp quyền micro để ghi âm.');
      }
    } catch (err) {
      console.error('Lỗi khi ghi âm', err);
    }
  };

  const stopRecording = async () => {
    try {
      await recorder.stop();
      setRecordedUri(recorder.uri);
    } catch (err) {
      console.error('Lỗi khi dừng ghi âm', err);
    }
  };

  const playRecording = () => {
    if (!recordedUri) return;
    if (isPlaying) {
      player.pause();
    } else {
      if (playerStatus.currentTime >= playerStatus.duration && playerStatus.duration > 0) {
        player.seekTo(0);
      }
      player.play();
    }
  };

  const formatDuration = (millis) => {
    if (!millis) return '00:00';
    const totalSeconds = Math.floor(millis / 1000);
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
  };

  const getFileName = (uri) => {
    if (!uri) return '';
    const parts = uri.split('/');
    return parts[parts.length - 1];
  };

  const handleUpdate = async () => {
    if (!fullName) {
      Alert.alert('Thiếu thông tin', 'Vui lòng nhập họ và tên.');
      return;
    }

    if (fullName === user.fullname && !recordedUri) {
      Alert.alert('Không có thay đổi', 'Bạn chưa thay đổi thông tin nào.');
      return;
    }

    setIsLoading(true);

    try {
      const formData = new FormData();
      formData.append('user_id', user.user_id);
      
      if (fullName !== user.fullname) {
        formData.append('fullname', fullName);
      }

      if (recordedUri) {
        const fileName = getFileName(recordedUri);
        const fileExtension = fileName.split('.').pop().toLowerCase();
        const mimeType = fileExtension === 'wav' ? 'audio/wav' : 'audio/mp4';

        formData.append('audio', {
          uri: Platform.OS === 'android' ? recordedUri : recordedUri.replace('file://', ''),
          name: fileName,
          type: mimeType,
        });
      }

      const response = await fetch('http://192.168.1.4:5000/changeInformation', {
        method: 'POST',
        headers: {
          'Content-Type': 'multipart/form-data',
        },
        body: formData,
      });

      const result = await response.json();

      if (response.ok && result.status === 'success') {
        // Cập nhật context
        login({
          ...user,
          fullname: result.new_fullname || fullName,
        });

        Alert.alert(
          'Thành công',
          'Đã cập nhật thông tin thành công!',
          [
            {
              text: 'OK',
              onPress: () => navigation.goBack(),
            },
          ]
        );
      } else {
        Alert.alert('Cập nhật thất bại', result.message || 'Đã có lỗi xảy ra.');
      }
    } catch (error) {
      console.error('[Update] Lỗi:', error);
      Alert.alert('Lỗi', 'Không thể kết nối tới server.');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <SafeAreaView className="flex-1 bg-background">
      <KeyboardAvoidingView
        className="flex-1"
        behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
      >
        <ScrollView contentContainerStyle={{ flexGrow: 1, padding: 24 }}>
          
          <View className="flex-row items-center mb-8">
            <TouchableOpacity onPress={() => navigation.goBack()} className="mr-4">
              <Ionicons name="arrow-back" size={28} color="#FFFFFF" />
            </TouchableOpacity>
            <Text className="text-2xl font-bold text-text">Chỉnh sửa thông tin</Text>
          </View>

          <View className="mb-6">
            <Text className="text-sm font-semibold text-textSecondary mb-2 ml-1">Họ và Tên</Text>
            <View className="flex-row items-center bg-card border border-border rounded-2xl px-4 h-14">
              <Ionicons name="person-outline" size={20} color="#8A8AA3" className="mr-3" />
              <TextInput
                className="flex-1 text-text text-base ml-2 h-full"
                placeholder="Nhập họ và tên"
                placeholderTextColor="#5A5A7A"
                value={fullName}
                onChangeText={setFullName}
                editable={!isLoading}
              />
            </View>
          </View>

          {/* Voice Recording Section */}
          <View className="mb-8 p-4 bg-card rounded-2xl border border-border">
            <Text className="text-base font-bold text-text mb-1">Cập nhật giọng nói</Text>
            <Text className="text-xs text-textSecondary mb-4">Không bắt buộc. Nếu bạn thu âm mới, giọng nói cũ sẽ bị xóa.</Text>

            <View className="flex-row items-center justify-between">
              <TouchableOpacity
                className={`flex-1 h-12 rounded-xl justify-center items-center flex-row ${recordedUri ? 'mr-2' : ''} ${isRecording ? 'bg-[#FF6B8A]/20 border border-[#FF6B8A]/50' : 'bg-[#6C63FF]/20 border border-[#6C63FF]/50'}`}
                onPress={isRecording ? stopRecording : startRecording}
              >
                <Ionicons name={isRecording ? "stop-circle" : "mic"} size={20} color={isRecording ? "#FF6B8A" : "#6C63FF"} />
                <Text className={`ml-2 font-semibold ${isRecording ? 'text-[#FF6B8A]' : 'text-[#6C63FF]'}`}>
                  {isRecording ? `Dừng (${formatDuration(recorderState.durationMillis)})` : "Ghi âm mới"}
                </Text>
              </TouchableOpacity>

              {recordedUri && !isRecording && (
                <TouchableOpacity
                  className="h-12 px-4 rounded-xl justify-center items-center flex-row bg-[#00D9A6]/20 border border-[#00D9A6]/50"
                  onPress={playRecording}
                >
                  <Ionicons name={isPlaying ? "pause" : "play"} size={20} color="#00D9A6" />
                  <Text className="ml-2 font-semibold text-[#00D9A6]">
                    {isPlaying ? "Đang phát..." : "Nghe lại"}
                  </Text>
                </TouchableOpacity>
              )}
            </View>
          </View>

          <TouchableOpacity
            className={`w-full h-14 rounded-2xl justify-center items-center mt-auto mb-6 shadow-lg shadow-primary/30 ${isLoading ? 'bg-primary/60' : 'bg-primary'}`}
            onPress={handleUpdate}
            activeOpacity={0.8}
            disabled={isLoading}
          >
            {isLoading ? (
              <View className="flex-row items-center">
                <ActivityIndicator color="#FFFFFF" size="small" />
                <Text className="text-white text-lg font-bold ml-3">Đang lưu...</Text>
              </View>
            ) : (
              <Text className="text-white text-lg font-bold">Lưu thay đổi</Text>
            )}
          </TouchableOpacity>

        </ScrollView>
      </KeyboardAvoidingView>
    </SafeAreaView>
  );
}
