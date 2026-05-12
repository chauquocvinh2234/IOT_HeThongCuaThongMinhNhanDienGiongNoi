import React, { useState } from 'react';
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
import { useAuth } from '../contexts/AuthContext';

export default function LoginScreen({ navigation }) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const { login } = useAuth();

  const handleLogin = async () => {
    if (!username || !password) {
      Alert.alert('Thiếu thông tin', 'Vui lòng nhập đầy đủ tên đăng nhập và mật khẩu.');
      return;
    }

    setIsLoading(true);

    try {
      const response = await fetch('http://192.168.1.4:5000/login', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ username, password }),
      });

      const result = await response.json();

      if (response.ok && result.status === 'success') {
        // Lưu thông tin user vào context
        login(result.user);

        // Chuyển sang màn hình chính
        navigation.replace('Main');
      } else {
        Alert.alert('Đăng nhập thất bại', result.message || 'Tên đăng nhập hoặc mật khẩu không đúng.');
      }
    } catch (error) {
      console.error('[Login] Lỗi kết nối:', error);
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
        <ScrollView contentContainerStyle={{ flexGrow: 1, justifyContent: 'center', padding: 24 }}>

          <View className="items-center mb-10">
            <View className="w-20 h-20 bg-primary/20 rounded-full justify-center items-center mb-4">
              <Ionicons name="home-outline" size={40} color="#6C63FF" />
            </View>
            <Text className="text-3xl font-bold text-text mb-2">Đăng nhập</Text>
            <Text className="text-sm text-textSecondary text-center">
              Chào mừng bạn trở lại với hệ thống Nhà thông minh
            </Text>
          </View>

          <View className="mb-6">
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
                editable={!isLoading}
              />
            </View>
          </View>

          <View className="mb-8">
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
                editable={!isLoading}
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

          <TouchableOpacity
            className={`w-full h-14 rounded-2xl justify-center items-center mb-6 shadow-lg shadow-primary/30 ${isLoading ? 'bg-primary/60' : 'bg-primary'}`}
            onPress={handleLogin}
            activeOpacity={0.8}
            disabled={isLoading}
          >
            {isLoading ? (
              <View className="flex-row items-center">
                <ActivityIndicator color="#FFFFFF" size="small" />
                <Text className="text-white text-lg font-bold ml-3">Đang đăng nhập...</Text>
              </View>
            ) : (
              <Text className="text-white text-lg font-bold">Đăng nhập</Text>
            )}
          </TouchableOpacity>

          <View className="flex-row justify-center mt-4">
            <Text className="text-textSecondary text-sm">Chưa có tài khoản? </Text>
            <TouchableOpacity onPress={() => navigation.navigate('Register')}>
              <Text className="text-primary text-sm font-bold">Đăng ký ngay</Text>
            </TouchableOpacity>
          </View>

        </ScrollView>
      </KeyboardAvoidingView>
    </SafeAreaView>
  );
}
