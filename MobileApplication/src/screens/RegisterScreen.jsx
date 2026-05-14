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
import * as Sharing from 'expo-sharing';
import * as FileSystem from 'expo-file-system/legacy';
import { StorageAccessFramework } from 'expo-file-system/legacy';
import { useAuth } from '../contexts/AuthContext';

export default function RegisterScreen({ navigation }) {
  const [fullName, setFullName] = useState('');
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const { login } = useAuth();

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

  const handleDownloadFile = async () => {
    if (!recordedUri) return;
    try {
      if (Platform.OS === 'android') {
        const permissions = await StorageAccessFramework.requestDirectoryPermissionsAsync();
        if (permissions.granted) {
          const base64 = await FileSystem.readAsStringAsync(recordedUri, { encoding: FileSystem.EncodingType.Base64 });
          const filename = getFileName(recordedUri);
          const mimeType = filename.endsWith('.wav') ? 'audio/wav' : 'audio/mp4';
          const newUri = await StorageAccessFramework.createFileAsync(permissions.directoryUri, filename, mimeType);
          await FileSystem.writeAsStringAsync(newUri, base64, { encoding: FileSystem.EncodingType.Base64 });
          Alert.alert('Thành công', 'File đã được lưu trực tiếp vào thư mục bạn chọn!');
        } else {
          Alert.alert('Từ chối', 'Bạn cần cấp quyền và chọn thư mục để ứng dụng lưu file.');
        }
      } else {
        const isAvailable = await Sharing.isAvailableAsync();
        if (isAvailable) {
          await Sharing.shareAsync(recordedUri);
        } else {
          Alert.alert('Không hỗ trợ', 'Tính năng chia sẻ/lưu file không khả dụng trên thiết bị này.');
        }
      }
    } catch (error) {
      console.error('Lỗi khi lưu file:', error);
      Alert.alert('Lỗi', 'Đã xảy ra lỗi khi xử lý file.');
    }
  };

  const handleRegister = async () => {
    if (!fullName || !username || !password || !confirmPassword || !recordedUri) {
      Alert.alert('Thiếu thông tin', 'Vui lòng điền đủ thông tin và ghi âm giọng nói.');
      return;
    }

    if (password !== confirmPassword) {
      Alert.alert('Mật khẩu không khớp', 'Mật khẩu nhập lại không trùng khớp. Vui lòng kiểm tra lại.');
      return;
    }

    setIsLoading(true);

    try {
      // Lấy tên file và xác định MIME type
      const fileName = getFileName(recordedUri);
      const fileExtension = fileName.split('.').pop().toLowerCase();
      const mimeType = fileExtension === 'wav' ? 'audio/wav' : 'audio/mp4';

      // Tạo FormData để gửi lên server
      const formData = new FormData();
      formData.append('username', username);
      formData.append('password', password);
      formData.append('fullname', fullName);
      formData.append('audio', {
        uri: recordedUri,
        type: mimeType,
        name: fileName,
      });

      console.log('[Register] Đang gửi dữ liệu tới server...');
      console.log(`  -> File: ${fileName} (${mimeType})`);

      const response = await fetch(`${process.env.EXPO_PUBLIC_API_URL}/register`, {
        method: 'POST',
        headers: {
          'Content-Type': 'multipart/form-data',
        },
        body: formData,
      });

      const result = await response.json();

      if (response.ok && result.status === 'success') {
        // Lưu thông tin user vào context
        login({
          user_id: result.user_id,
          username: username,
          fullname: fullName,
        });

        Alert.alert(
          'Đăng ký thành công!',
          result.message || `Chào mừng ${fullName}!`,
          [
            {
              text: 'Tiếp tục',
              onPress: () => navigation.replace('Main'),
            },
          ]
        );
      } else if (response.status === 409) {
        Alert.alert('Tài khoản đã tồn tại', result.message || 'Tên đăng nhập này đã được sử dụng.');
      } else {
        Alert.alert('Lỗi đăng ký', result.message || 'Đã xảy ra lỗi, vui lòng thử lại.');
      }
    } catch (error) {
      console.error('[Register] Lỗi kết nối:', error);
      Alert.alert(
        'Lỗi kết nối',
        'Không thể kết nối tới server. Vui lòng kiểm tra:\n• Server đang chạy tại 192.168.1.4:5000\n• Điện thoại và máy tính cùng mạng WiFi'
      );
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

          <TouchableOpacity
            className="w-10 h-10 rounded-full bg-card border border-border justify-center items-center mb-6"
            onPress={() => navigation.goBack()}
          >
            <Ionicons name="arrow-back" size={20} color="#FFFFFF" />
          </TouchableOpacity>

          <View className="mb-8">
            <Text className="text-3xl font-bold text-text mb-2">Đăng ký</Text>
            <Text className="text-sm text-textSecondary">
              Tạo tài khoản và thu âm giọng nói để xác thực.
            </Text>
          </View>

          <View className="mb-5">
            <Text className="text-sm font-semibold text-textSecondary mb-2 ml-1">Họ và tên</Text>
            <View className="flex-row items-center bg-card border border-border rounded-2xl px-4 h-14">
              <Ionicons name="person-circle-outline" size={20} color="#8A8AA3" className="mr-3" />
              <TextInput
                className="flex-1 text-text text-base ml-2 h-full"
                placeholder="Nhập họ và tên"
                placeholderTextColor="#5A5A7A"
                value={fullName}
                onChangeText={setFullName}
              />
            </View>
          </View>

          <View className="mb-5">
            <Text className="text-sm font-semibold text-textSecondary mb-2 ml-1">Tên đăng nhập</Text>
            <View className="flex-row items-center bg-card border border-border rounded-2xl px-4 h-14">
              <Ionicons name="person-outline" size={20} color="#8A8AA3" className="mr-3" />
              <TextInput
                className="flex-1 text-text text-base ml-2 h-full"
                placeholder="Nhập tên đăng nhập"
                placeholderTextColor="#5A5A7A"
                value={username}
                onChangeText={setUsername}
                autoCapitalize="none"
              />
            </View>
          </View>

          <View className="mb-6">
            <Text className="text-sm font-semibold text-textSecondary mb-2 ml-1">Mật khẩu</Text>
            <View className="flex-row items-center bg-card border border-border rounded-2xl px-4 h-14">
              <Ionicons name="lock-closed-outline" size={20} color="#8A8AA3" className="mr-3" />
              <TextInput
                className="flex-1 text-text text-base ml-2 h-full"
                placeholder="Nhập mật khẩu"
                placeholderTextColor="#5A5A7A"
                value={password}
                onChangeText={setPassword}
                secureTextEntry={!showPassword}
              />
              <TouchableOpacity onPress={() => setShowPassword(!showPassword)}>
                <Ionicons
                  name={showPassword ? "eye-off-outline" : "eye-outline"}
                  size={20}
                  color="#8A8AA3"
                />
              </TouchableOpacity>
            </View>
          </View>

          <View className="mb-6">
            <Text className="text-sm font-semibold text-textSecondary mb-2 ml-1">Nhập lại mật khẩu</Text>
            <View className="flex-row items-center bg-card border border-border rounded-2xl px-4 h-14">
              <Ionicons name="lock-closed-outline" size={20} color="#8A8AA3" className="mr-3" />
              <TextInput
                className="flex-1 text-text text-base ml-2 h-full"
                placeholder="Nhập lại mật khẩu"
                placeholderTextColor="#5A5A7A"
                value={confirmPassword}
                onChangeText={setConfirmPassword}
                secureTextEntry={!showConfirmPassword}
              />
              <TouchableOpacity onPress={() => setShowConfirmPassword(!showConfirmPassword)}>
                <Ionicons
                  name={showConfirmPassword ? "eye-off-outline" : "eye-outline"}
                  size={20}
                  color="#8A8AA3"
                />
              </TouchableOpacity>
            </View>
            {confirmPassword !== '' && password !== confirmPassword && (
              <Text className="text-xs mt-1.5 ml-1" style={{ color: '#FF6B8A' }}>
                Mật khẩu không khớp
              </Text>
            )}
          </View>

          {/* Voice Recording Section */}
          <View className="mb-8 p-4 bg-card rounded-2xl border border-border">
            <Text className="text-base font-bold text-text mb-1">Ghi âm giọng nói</Text>
            <Text className="text-xs text-textSecondary mb-4">Định dạng 16kHz, 1 mono</Text>

            <View className="flex-row items-center justify-between">
              <TouchableOpacity
                className={`flex-1 h-12 rounded-xl justify-center items-center flex-row ${recordedUri ? 'mr-2' : ''} ${isRecording ? 'bg-accent/20 border border-accent/50' : 'bg-primary/20 border border-primary/50'}`}
                onPress={isRecording ? stopRecording : startRecording}
              >
                <Ionicons name={isRecording ? "stop-circle" : "mic"} size={20} color={isRecording ? "#FF6B8A" : "#6C63FF"} />
                <Text className={`ml-2 font-semibold ${isRecording ? 'text-accent' : 'text-primary'}`}>
                  {isRecording ? `Dừng ghi âm (${formatDuration(recorderState.durationMillis)})` : "Ghi âm"}
                </Text>
              </TouchableOpacity>

              {recordedUri && (
                <TouchableOpacity
                  className={`w-12 h-12 rounded-xl justify-center items-center ${isPlaying ? 'bg-red-500/20 border-2 border-red-500' : 'bg-cardLight border border-border'}`}
                  onPress={playRecording}
                >
                  <Ionicons name={isPlaying ? "pause" : "play"} size={20} color={isPlaying ? "#ef4444" : "#FFFFFF"} />
                </TouchableOpacity>
              )}
            </View>

            {recordedUri && (
              <TouchableOpacity
                className="mt-4 flex-row items-center justify-center p-3 bg-secondary/10 rounded-xl border border-secondary/30"
                onPress={handleDownloadFile}
              >
                <Ionicons name="document-text-outline" size={16} color="#00D9A6" style={{ marginRight: 6 }} />
                <Text className="text-sm font-medium text-secondary mx-1 flex-shrink" numberOfLines={1} ellipsizeMode="middle">
                  {getFileName(recordedUri)}
                </Text>
                <Ionicons name="download-outline" size={16} color="#00D9A6" style={{ marginLeft: 6 }} />
              </TouchableOpacity>
            )}
          </View>

          <TouchableOpacity
            className={`w-full h-14 rounded-2xl justify-center items-center mb-6 shadow-lg shadow-primary/30 ${isLoading ? 'bg-primary/50' : 'bg-primary'}`}
            onPress={handleRegister}
            activeOpacity={0.8}
            disabled={isLoading}
          >
            {isLoading ? (
              <View className="flex-row items-center">
                <ActivityIndicator color="#FFFFFF" size="small" />
                <Text className="text-white text-lg font-bold ml-3">Đang đăng ký...</Text>
              </View>
            ) : (
              <Text className="text-white text-lg font-bold">Đăng ký & Đăng nhập</Text>
            )}
          </TouchableOpacity>

        </ScrollView>
      </KeyboardAvoidingView>
    </SafeAreaView>
  );
}
