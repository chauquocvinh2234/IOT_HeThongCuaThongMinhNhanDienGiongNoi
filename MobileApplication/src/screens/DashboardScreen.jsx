import React, { useRef, useEffect, useState } from 'react';
import {
  View,
  Text,
  Animated,
  TouchableOpacity,
  Alert,
  ActivityIndicator,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useAuth } from '../contexts/AuthContext';

const COLORS = {
  primary: '#6C63FF',
  primaryLight: '#8B83FF',
  secondary: '#00D9A6',
  background: '#0F0F1A',
  card: '#1A1A2E',
  text: '#FFFFFF',
  textSecondary: '#8A8AA3',
  border: '#2A2A4A',
};

export default function DashboardScreen() {
  const { user } = useAuth();
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const slideAnim = useRef(new Animated.Value(40)).current;
  const scaleAnim = useRef(new Animated.Value(0.8)).current;

  const [isOpening, setIsOpening] = useState(false);

  const handleOpenDoor = async () => {
    setIsOpening(true);
    try {
      const response = await fetch(`${process.env.EXPO_PUBLIC_API_URL}/remote_control`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          action: 'open',
          user_id: user?.user_id
        }),
      });

      const result = await response.json();

      if (response.ok && result.status === 'success') {
        Alert.alert('Thành công', 'Đã mở cửa thành công!');
      } else {
        Alert.alert('Thất bại', result.message || 'Không thể mở cửa.');
      }
    } catch (error) {
      console.error('[Remote Control] Lỗi:', error);
      Alert.alert('Lỗi kết nối', 'Không thể kết nối tới server.');
    } finally {
      setIsOpening(false);
    }
  };

  useEffect(() => {
    Animated.parallel([
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 1000,
        useNativeDriver: true,
      }),
      Animated.spring(slideAnim, {
        toValue: 0,
        tension: 50,
        friction: 8,
        useNativeDriver: true,
      }),
      Animated.spring(scaleAnim, {
        toValue: 1,
        tension: 50,
        friction: 6,
        useNativeDriver: true,
      }),
    ]).start();
  }, []);

  const displayName = user?.fullname || 'Người dùng';

  return (
    <SafeAreaView className="flex-1 bg-background">
      <View className="flex-1 justify-center items-center px-8">

        {/* Animated Welcome Icon */}
        <Animated.View
          style={{
            opacity: fadeAnim,
            transform: [{ scale: scaleAnim }],
          }}
        >
          <View
            className="w-28 h-28 rounded-full justify-center items-center mb-8"
            style={{ backgroundColor: COLORS.primary + '20' }}
          >
            <View
              className="w-20 h-20 rounded-full justify-center items-center"
              style={{ backgroundColor: COLORS.primary + '35' }}
            >
              <Ionicons name="home" size={40} color={COLORS.primary} />
            </View>
          </View>
        </Animated.View>

        {/* Welcome Text */}
        <Animated.View
          style={{
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
            alignItems: 'center',
          }}
        >
          <Text className="text-base text-textSecondary mb-2">
            Xin chào 👋
          </Text>
          <Text className="text-3xl font-bold text-text text-center mb-3">
            Chào mừng{'\n'}
            <Text style={{ color: COLORS.primary }}>{displayName}</Text>
            {'\n'}quay trở lại!
          </Text>
          <Text className="text-sm text-textSecondary text-center mt-2 leading-5">
            Hệ thống Cửa nhà Thông minh đang hoạt động bình thường
          </Text>
        </Animated.View>

        {/* Status Indicator */}
        <Animated.View
          className="mt-10 flex-row items-center bg-card rounded-2xl px-6 py-4 border border-border"
          style={{
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
          }}
        >
          <View
            className="w-3 h-3 rounded-full mr-3"
            style={{ backgroundColor: COLORS.secondary }}
          />
          <Text className="text-sm text-textSecondary">
            Hệ thống đang{' '}
            <Text style={{ color: COLORS.secondary, fontWeight: '600' }}>trực tuyến</Text>
          </Text>
        </Animated.View>

        {/* Nút Mở Cửa Từ Xa */}
        <Animated.View
          className="mt-8 w-full"
          style={{
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
          }}
        >
          <TouchableOpacity
            className={`w-full h-16 rounded-2xl flex-row justify-center items-center shadow-lg shadow-primary/30 ${isOpening ? 'bg-primary/60' : 'bg-primary'}`}
            onPress={handleOpenDoor}
            disabled={isOpening}
            activeOpacity={0.8}
          >
            {isOpening ? (
              <>
                <ActivityIndicator color="#FFFFFF" size="small" />
                <Text className="text-white text-lg font-bold ml-3">Đang mở...</Text>
              </>
            ) : (
              <>
                <Ionicons name="key-outline" size={24} color="#FFFFFF" />
                <Text className="text-white text-lg font-bold ml-2">Mở Cửa Từ Xa</Text>
              </>
            )}
          </TouchableOpacity>
        </Animated.View>

      </View>
    </SafeAreaView>
  );
}
